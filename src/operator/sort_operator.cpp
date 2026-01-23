#include "operator/sort_operator.hpp"

#include "catalog/row_batch.hpp"
#include "common/constants.hpp"
#include "expression/expression.hpp"

namespace velodb {

// SortOperator implementation
SortOperator::SortOperator(ExecutionContext& context,
                           Schema output_schema,
                           std::unique_ptr<AbstractOperator> child,
                           std::vector<size_t> order_indices,
                           std::vector<bool> ascending_flags)
    : UnaryOperator(context, std::move(output_schema), std::move(child))
    , order_indices_(std::move(order_indices))
    , ascending_flags_(std::move(ascending_flags))
{
}

Result<RowBatch> SortOperator::next()
{
    PROFILE_SCOPE("SortOperator::next");
    if (!sorted_) {
        RowBatch gathered_batch = collectBatches(*child_);
        if (gathered_batch.getRowCount() == 0) {
            return Result<RowBatch>::success(RowBatch()); // End of stream
        }
        gathered_batch.to(DataLocation::CUDA);
        size_t n_rows = gathered_batch.getRowCount();
        size_t n_padded_rows = nextPow2(n_rows);
        size_t n_cols = gathered_batch.getColumnCount();
        auto& task_manager = context_.getTaskManager();
        auto stream_handle = StreamPool::getInstance().acquire().value();

        // Perform sorting
        size_t n_sort_columns = order_indices_.size();
        int32_t* d_indices;
        CHECKED_CALL_THROW(cudaMallocAsync(&d_indices, n_padded_rows * sizeof(int32_t), stream_handle->get()));
        stream_handle->synchronize();
        std::vector<bool> sorted_cols(n_sort_columns, false);
        uint64_t last_id;
        Command sort_cmd;
        sort_cmd.opcode = OpCode::OP_SORT;
        sort_cmd.args = { .sort = {
                              .sort_cols = {},
                              .indices = d_indices,
                              .n_sort_columns = n_sort_columns,
                              .n_rows = n_rows,
                              .n_padded_rows = n_padded_rows,
                              .col_types = {},
                              .ascending_flags = {},
                          } };
        for (size_t i = 0; i < n_sort_columns; ++i) {
            size_t col_idx = order_indices_[i];
            auto& col = gathered_batch.getColumn(col_idx);
            sorted_cols[col_idx] = true;
            sort_cmd.args.sort.sort_cols[i] = col.rawData();
            sort_cmd.args.sort.col_types[i] = col.getType().getTypeId();
            sort_cmd.args.sort.ascending_flags[i] = ascending_flags_[i];
        }
        last_id = task_manager.submitCommand(sort_cmd);
        task_manager.waitCommand(last_id);

        // Permute unsorted columns
        std::vector<void*> buffers(n_cols, nullptr);
        std::vector<BitVector::Element*> bitmap_buffers(n_cols, nullptr);
        for (size_t i = 0; i < n_cols; ++i) {
            if (sorted_cols[i]) {
                continue; // Already sorted
            }
            auto& col = gathered_batch.getColumn(i);
            auto* data_ptr = col.rawData();
            auto* bitmap_ptr = col.rawBitmapData();
            auto* temp_buffer = col.getDeviceBuffer();
            auto* temp_bitmap_buffer = col.getDeviceBitmapBuffer();
            buffers[i] = temp_buffer;
            bitmap_buffers[i] = temp_bitmap_buffer;

            Command gather_cmd;
            gather_cmd.opcode = OpCode::OP_PERMUTE;
            gather_cmd.args = { .permute = {
                                    .out_data = static_cast<void*>(temp_buffer),
                                    .in_data = static_cast<void*>(data_ptr),
                                    .in_indices = d_indices,
                                    .n = n_rows,
                                    .type_id = col.getType().getTypeId(),
                                } };
            task_manager.submitCommand(gather_cmd);

            Command gather_bits_cmd;
            gather_bits_cmd.opcode = OpCode::OP_PERMUTE;
            gather_bits_cmd.args = { .permute = {
                                         .out_data = static_cast<void*>(temp_bitmap_buffer),
                                         .in_data = static_cast<void*>(bitmap_ptr),
                                         .in_indices = d_indices,
                                         .n = n_rows,
                                         .type_id = DataTypeId::BOOLEAN,
                                     } };
            last_id = task_manager.submitCommand(gather_bits_cmd);
        }
        task_manager.waitCommand(last_id);

        for (size_t i = 0; i < n_cols; ++i) {
            if (buffers[i] == nullptr) {
                continue;
            }
            auto& col = gathered_batch.getColumn(i);
            col.setFromDeviceBuffers(buffers[i], bitmap_buffers[i]);
        }
        CHECKED_CALL_THROW(cudaFreeAsync(d_indices, stream_handle->get()));
        sorted_ = true;
        return Result<RowBatch>::success(std::move(gathered_batch));
    }
    return Result<RowBatch>::success(RowBatch());
}

} // namespace velodb
