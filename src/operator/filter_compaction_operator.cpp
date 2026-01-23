#include "operator/filter_compaction_operator.hpp"

#include "catalog/column.hpp"
#include "catalog/row_batch.hpp"
#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include "common/constants.hpp"
#include "common/result.hpp"
#include "cuda/commands.hpp"

#include <cuda_runtime.h>

namespace velodb {

FilterCompactionOperator::FilterCompactionOperator(ExecutionContext& context,
                                                   Schema output_schema,
                                                   std::unique_ptr<AbstractOperator> child)
    : UnaryOperator(context, std::move(output_schema), std::move(child))
{
}

Result<RowBatch> FilterCompactionOperator::next()
{
    PROFILE_SCOPE("FilterCompactionOperator::next");

    auto child_result = child_->next();
    if (!child_result) {
        return child_result; // Propagate error from child
    }
    auto& batch = child_result.value();
    batch.to(DataLocation::CUDA);
    size_t n_rows = batch.getRowCount();
    if (n_rows == 0) {
        return child_result; // End of stream
    }
    auto& task_manager = context_.getTaskManager();

    // We assume $_mask always exists
    auto mask_index = output_schema_.getColumnIndex("$_mask");
    auto& mask_column = batch.getColumn(mask_index);
    auto stream_handler = StreamPool::getInstance().acquire().value();
    const uint8_t* mask_data = static_cast<const uint8_t*>(mask_column.rawData());
    int32_t* scatter_indices = MemoryAllocator::allocate<int32_t>(DataLocation::CUDA,
                                                                  batch.getRowCount(),
                                                                  stream_handler->get());
    size_t* scatter_count = MemoryAllocator::allocate<size_t>(DataLocation::CUDA, 1, stream_handler->get());
    CHECKED_CALL_THROW(cudaMemsetAsync(scatter_count, 0, sizeof(size_t), stream_handler->get()));
    stream_handler->synchronize();

    // Scatter command to compact rows based on mask
    uint64_t last_id;
    Command scatter_cmd = {};
    scatter_cmd.opcode = OpCode::OP_SCATTER;
    scatter_cmd.args = {
            .scatter = {
                .out_indices = scatter_indices,
                .out_count = scatter_count,
                .in_mask = mask_data,
                .n = n_rows,
            },
        };
    last_id = task_manager.submitCommand(scatter_cmd);

    // Gather command to collect the valid rows
    size_t n_cols = batch.getColumnCount();
    std::vector<void*> buffers;
    std::vector<BitVector::Element*> bitmap_buffers;
    buffers.reserve(n_cols);
    bitmap_buffers.reserve(n_cols);
    for (auto& input_col : batch.getColumns()) {
        auto* data_ptr = input_col.rawData();
        auto* bitmap_ptr = input_col.rawBitmapData();
        auto* temp_buffer = input_col.getTemporaryBuffer();
        auto* temp_bitmap_buffer = input_col.getTemporaryBitmapBuffer();
        buffers.push_back(temp_buffer);
        bitmap_buffers.push_back(temp_bitmap_buffer);

        Command gather_cmd = {};
        gather_cmd.opcode = OpCode::OP_GATHER;
        gather_cmd.args = {
            .gather = {
                .out_data = static_cast<void*>(temp_buffer),
                .in_data = static_cast<void*>(data_ptr),
                .in_indices = scatter_indices,
                .in_mask = mask_data,
                .n = n_rows,
                .type_id = input_col.getType().getTypeId(),
            },
        };
        task_manager.submitCommand(gather_cmd);

        Command gather_bits_cmd = {};
        gather_bits_cmd.opcode = OpCode::OP_GATHER;
        gather_bits_cmd.args = { .gather = {
                                     .out_data = static_cast<void*>(temp_bitmap_buffer),
                                     .in_data = static_cast<void*>(bitmap_ptr),
                                     .in_indices = scatter_indices,
                                     .in_mask = mask_data,
                                     .n = n_rows,
                                     .type_id = DataTypeId::BOOLEAN,
                                 } };
        last_id = task_manager.submitCommand(gather_bits_cmd);
    }
    task_manager.waitCommand(last_id);

    size_t h_scatter_count = 0;
    CHECKED_CALL_THROW(cudaMemcpyAsync(&h_scatter_count,
                                       scatter_count,
                                       sizeof(size_t),
                                       cudaMemcpyDeviceToHost,
                                       stream_handler->get()));
    stream_handler->synchronize();
    for (size_t col_idx = 0; col_idx < n_cols; ++col_idx) {
        auto& input_col = batch.getColumn(col_idx);
        input_col.setFromBuffer(buffers[col_idx], bitmap_buffers[col_idx]);
    }
    setNumRowsForBatch(batch, h_scatter_count);

    MemoryAllocator::deallocate(scatter_indices);
    MemoryAllocator::deallocate(scatter_count);
    return Result<RowBatch>::success(std::move(batch));
}

} // namespace velodb
