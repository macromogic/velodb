#include "operator/limit_operator.hpp"

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
    , buffer_(RowBatch::createBuffered(output_schema_, MAX_BATCH_SIZE * 2, DataLocation::CUDA))
{
}

Result<RowBatch> LimitOperator::next()
{
    auto* child = getChild();
    if (!child) {
        return Result<RowBatch>::failure("LimitOperator requires a child operator");
    }
    while (true) {
        auto child_result = child->next();
        if (!child_result) {
            return child_result; // Propagate error from child
        }
        auto& input_batch = child_result.value();
        auto input_row_count = input_batch.getRowCount();
        if (input_row_count == 0) {
            // Produce the last (maybe incomplete) batch if we have any rows buffered
            auto last_batch_size = std::min(MAX_BATCH_SIZE, std::min(buffer_.getRowCount(), limit_ - current_count_));
            current_count_ += last_batch_size;
            return Result<RowBatch>::success(std::move(buffer_.splitFront(last_batch_size)));
        }

        if (skipped_count_ < offset_) {
            size_t to_skip = std::min(offset_ - skipped_count_, input_row_count);
            skipped_count_ += to_skip;
            if (to_skip == input_row_count) {
                continue; // Skip entire batch
            }
            // Adjust input batch to skip the rows
            input_batch.splitFront(to_skip);
        }

        // Discard batches beyond the limit
        if (current_count_ < limit_) {
            buffer_.addRows(input_batch);

            // If we can produce a full batch, do so
            if (std::min(buffer_.getRowCount(), limit_ - current_count_) >= MAX_BATCH_SIZE) {
                auto output_batch = buffer_.splitFront(MAX_BATCH_SIZE);
                current_count_ += MAX_BATCH_SIZE;
                return Result<RowBatch>::success(std::move(output_batch));
            }
        }
    }
}

} // namespace velodb
