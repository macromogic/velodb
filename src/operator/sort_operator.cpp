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
    // If we have sorted data, return next batch slice
    if (sorted_) {
        size_t total_rows = sorted_batch_.getRowCount();
        if (current_offset_ >= total_rows) {
            return Result<RowBatch>::success(RowBatch()); // End of stream
        }

        size_t batch_end = std::min(current_offset_ + MAX_BATCH_SIZE, total_rows);
        auto slice = sorted_batch_.slice(current_offset_, batch_end);
        // Keep data on CUDA - downstream operators will transfer as needed
        current_offset_ = batch_end;
        return Result<RowBatch>::success(std::move(slice));
    }

    // First call: collect all data, sort it, then return first batch
    RowBatch gathered_batch = collectBatches(*child_);
    if (gathered_batch.getRowCount() == 0) {
        sorted_ = true;
        return Result<RowBatch>::success(RowBatch()); // End of stream
    }

    PROFILE_SCOPE("SortOperator::next");
    gathered_batch.to(DataLocation::CUDA);
    size_t n_rows = gathered_batch.getRowCount();
    size_t n_padded_rows = nextPow2(n_rows);
    size_t n_cols = gathered_batch.getColumnCount();
    auto& task_manager = context_.getTaskManager();
    auto stream_handle = StreamPool::getInstance().acquire().value();

    // Perform sorting
    size_t n_sort_columns = order_indices_.size();
    int64_t* d_indices;
    CHECKED_CALL_THROW(cudaMallocAsync(&d_indices, n_padded_rows * sizeof(int64_t), stream_handle->get()));
    stream_handle->synchronize();
    std::vector<bool> sorted_cols(n_cols, false);
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

    // Permute unsorted columns - process one at a time to reduce peak GPU memory
    for (size_t i = 0; i < n_cols; ++i) {
        auto& col = gathered_batch.getColumn(i);
        void* data_buffer = nullptr;

        if (!sorted_cols[i]) {
            auto* data_ptr = col.rawData();
            auto* temp_buffer = col.getDeviceBuffer();
            data_buffer = temp_buffer;

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
        }

        auto* bitmap_ptr = col.rawBitmapData();
        auto* temp_bitmap_buffer = col.getDeviceBitmapBuffer();
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

        // Wait and swap buffers immediately to free old GPU memory
        task_manager.waitCommand(last_id);
        col.setFromDeviceBuffers(data_buffer, temp_bitmap_buffer);
    }

    CHECKED_CALL_THROW(cudaFreeAsync(d_indices, stream_handle->get()));

    // Store the sorted batch (kept on CUDA) and mark as sorted
    sorted_batch_ = std::move(gathered_batch);
    sorted_ = true;
    current_offset_ = 0;

    // Return first batch - keep on CUDA, downstream operators will transfer as needed
    size_t total_rows = sorted_batch_.getRowCount();
    size_t batch_end = std::min(current_offset_ + MAX_BATCH_SIZE, total_rows);
    auto slice = sorted_batch_.slice(current_offset_, batch_end);
    current_offset_ = batch_end;
    return Result<RowBatch>::success(std::move(slice));
}

} // namespace velodb
