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
    auto child_result = child_->next();
    if (!child_result) {
        return child_result; // Propagate error from child
    }
    PROFILE_SCOPE("FilterCompactionOperator::next");
    auto& batch = child_result.value();
    batch.to(DataLocation::CUDA);
    size_t n_rows = batch.getRowCount();
    if (n_rows == 0) {
        return child_result; // End of stream
    }
    auto& task_manager = context_.getTaskManager();

    // We assume $_mask always exists, possibly with a table prefix
    size_t mask_index = -1;
    bool found_mask = false;
    for (size_t i = 0; i < output_schema_.getColumnCount(); ++i) {
        auto name = output_schema_.getColumnInfo(i).getName();
        if (name == "$_mask" || (name.length() > 7 && name.substr(name.length() - 7) == ".$_mask")) {
            mask_index = i;
            found_mask = true;
            break;
        }
    }

    if (!found_mask) {
        // Fallback or explicit check
        mask_index = output_schema_.getColumnIndex("$_mask");
    }

    auto& mask_column = batch.getColumn(mask_index);
    auto stream_handle = StreamPool::getInstance().acquire().value();
    const uint8_t* mask_data = static_cast<const uint8_t*>(mask_column.rawData());
    int32_t* d_scatter_indices;
    CHECKED_CALL_THROW(
        cudaMallocAsync(&d_scatter_indices, batch.getRowCount() * sizeof(int32_t), stream_handle->get()));
    size_t* d_scatter_count;
    CHECKED_CALL_THROW(cudaMallocAsync(&d_scatter_count, 1 * sizeof(size_t), stream_handle->get()));
    CHECKED_CALL_THROW(cudaMemsetAsync(d_scatter_count, 0, sizeof(size_t), stream_handle->get()));
    stream_handle->synchronize();

    // Scatter command to compact rows based on mask
    uint64_t last_id;
    Command scatter_cmd = {};
    scatter_cmd.opcode = OpCode::OP_SCATTER;
    scatter_cmd.args = {
            .scatter = {
                .out_indices = d_scatter_indices,
                .out_count = d_scatter_count,
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
        auto* temp_buffer = input_col.getDeviceBuffer();
        auto* temp_bitmap_buffer = input_col.getDeviceBitmapBuffer();
        buffers.push_back(temp_buffer);
        bitmap_buffers.push_back(temp_bitmap_buffer);

        Command gather_cmd = {};
        gather_cmd.opcode = OpCode::OP_GATHER;
        gather_cmd.args = {
            .gather = {
                .out_data = static_cast<void*>(temp_buffer),
                .in_data = static_cast<void*>(data_ptr),
                .in_indices = d_scatter_indices,
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
                                     .in_indices = d_scatter_indices,
                                     .in_mask = mask_data,
                                     .n = n_rows,
                                     .type_id = DataTypeId::BOOLEAN,
                                 } };
        last_id = task_manager.submitCommand(gather_bits_cmd);
    }
    task_manager.waitCommand(last_id);

    size_t h_scatter_count = 0;
    CHECKED_CALL_THROW(cudaMemcpyAsync(&h_scatter_count,
                                       d_scatter_count,
                                       sizeof(size_t),
                                       cudaMemcpyDeviceToHost,
                                       stream_handle->get()));
    stream_handle->synchronize();
    for (size_t col_idx = 0; col_idx < n_cols; ++col_idx) {
        auto& input_col = batch.getColumn(col_idx);
        input_col.setFromDeviceBuffers(buffers[col_idx], bitmap_buffers[col_idx]);
    }
    setNumRowsForBatch(batch, h_scatter_count);

    CHECKED_CALL_THROW(cudaFreeAsync(d_scatter_indices, stream_handle->get()));
    CHECKED_CALL_THROW(cudaFreeAsync(d_scatter_count, stream_handle->get()));
    return Result<RowBatch>::success(std::move(batch));
}

} // namespace velodb
