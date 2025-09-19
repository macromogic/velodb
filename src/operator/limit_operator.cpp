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
            return Result<RowBatch>::success(std::move(buffer_.splitFront(buffer_.getRowCount())));
        }

        if (skipped_count_ < offset_) {
            size_t to_skip = std::min(offset_ - skipped_count_, input_row_count);
            skipped_count_ += to_skip;
            if (to_skip == input_row_count) {
                continue; // Skip entire batch
            }
            // Adjust input batch to skip the rows
            input_batch.splitFront(input_row_count - to_skip);
        }

        buffer_.addRows(input_batch);
        size_t next_batch_size = MAX_BATCH_SIZE;
        if (current_count_ < limit_) {
            next_batch_size = std::min(next_batch_size, limit_ - current_count_);
        }
        current_count_ += input_batch.getRowCount();
        if (next_batch_size > 0 && buffer_.getRowCount() >= next_batch_size) {
            auto output_batch = buffer_.splitFront(next_batch_size);
            return Result<RowBatch>::success(std::move(output_batch));
        }
    }
}

} // namespace velodb
