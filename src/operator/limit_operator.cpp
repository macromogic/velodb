#include "operator/limit_operator.hpp"

#include "common/constants.hpp"

namespace velodb {

// LimitOperator implementation
LimitOperator::LimitOperator(ExecutionContext& context,
                             Schema output_schema,
                             std::unique_ptr<AbstractOperator> child,
                             size_t limit,
                             size_t offset)
    : UnaryOperator(context, std::move(output_schema), std::move(child))
    , limit_(limit)
    , offset_(offset)
    , current_count_(0)
    , skipped_count_(0)
{
}

Result<RowBatch> LimitOperator::next()
{
    auto child_result = child_->next();
    if (!child_result) {
        return child_result; // Propagate error from child
    }
    PROFILE_SCOPE("Limit");
    auto input_batch = std::move(child_result).value();
    input_batch.to(DataLocation::CUDA);
    auto input_row_count = input_batch.getRowCount();
    auto n_cols = input_batch.getColumnCount();
    if (input_row_count == 0) {
        return child_result; // End of stream
    }

    size_t skip_count = 0;
    if (skipped_count_ < offset_) {
        skip_count = std::min(offset_ - skipped_count_, input_row_count);
        skipped_count_ += skip_count;
    }
    size_t rows_to_take = input_row_count - skip_count;
    if (current_count_ + rows_to_take > limit_) {
        rows_to_take = limit_ - current_count_;
    }
    current_count_ += rows_to_take;
    if (rows_to_take == 0) {
        return Result<RowBatch>::success(RowBatch()); // Limit reached
    }

    // Process columns one at a time to reduce peak GPU memory usage
    auto stream_handle = StreamPool::getInstance().acquire().value();
    for (size_t col_idx = 0; col_idx < n_cols; ++col_idx) {
        auto& col = input_batch.getColumn(col_idx);
        auto* data_ptr = col.rawData();
        auto* bitmap_ptr = col.rawBitmapData();
        auto* temp_buffer = col.getDeviceBuffer();
        auto* temp_bitmap_buffer = col.getDeviceBitmapBuffer();

        auto data_size = col.getType().size();
        CHECKED_CALL_THROW(cudaMemcpyAsync(temp_buffer,
                                           static_cast<const uint8_t*>(data_ptr) + skip_count * data_size,
                                           rows_to_take * data_size,
                                           cudaMemcpyDeviceToDevice,
                                           stream_handle->get()));

        CHECKED_CALL_THROW(
            cudaMemcpyAsync(temp_bitmap_buffer,
                            static_cast<const uint8_t*>(bitmap_ptr) + skip_count * sizeof(BitVector::Element),
                            rows_to_take * sizeof(BitVector::Element),
                            cudaMemcpyDeviceToDevice,
                            stream_handle->get()));

        // Synchronize and swap buffers immediately to free old GPU memory
        stream_handle->synchronize();
        col.setFromDeviceBuffers(temp_buffer, temp_bitmap_buffer);
    }

    setNumRowsForBatch(input_batch, rows_to_take);
    return Result<RowBatch>::success(std::move(input_batch));
}

} // namespace velodb
