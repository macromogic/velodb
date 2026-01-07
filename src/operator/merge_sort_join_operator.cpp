#include "operator/merge_sort_join_operator.hpp"

#include "expression/expression.hpp"

namespace velodb {

MergeSortJoinOperator::MergeSortJoinOperator(ExecutionContext& context,
                                             Schema output_schema,
                                             std::unique_ptr<AbstractOperator> left_child,
                                             std::unique_ptr<AbstractOperator> right_child,
                                             std::unique_ptr<AbstractExpression> left_key_expr,
                                             std::unique_ptr<AbstractExpression> right_key_expr,
                                             JoinType join_type)
    : BinaryOperator(context, std::move(output_schema), std::move(left_child), std::move(right_child))
    , left_key_expr_(std::move(left_key_expr))
    , right_key_expr_(std::move(right_key_expr))
    , join_type_(join_type)
    , left_buffer_(RowBatch::createBuffered(getLeftChild()->getOutputSchema(), MAX_BATCH_SIZE * 2, DataLocation::CUDA))
    , right_buffer_(
          RowBatch::createBuffered(getRightChild()->getOutputSchema(), MAX_BATCH_SIZE * 2, DataLocation::CUDA))
{
}

Result<RowBatch> MergeSortJoinOperator::next()
{
    PROFILE_SCOPE("MergeSortJoinOperator::next");
    // Simple single-pass INNER merge equi-join on first column (join key) of each side.
    // Assumptions:
    //  Left & Right inputs are individually sorted ascending by key (column 0).
    //  Column layout per side after projection: [key, $_rowid, $_mask].
    // Output schema: [left_table_$_rowid, right_table_$_rowid]

    auto* left = getLeftChild();
    auto* right = getRightChild();
    if (!left || !right) {
        return Result<RowBatch>::failure("MergeSortJoinOperator requires two child operators");
    }

    // Static state for one-shot implementation (no resume). For now we materialize all pairs then stream.
    // Future optimization: incremental streaming with retained cursors.
    if (joined_) {
        // Return empty batch to signal completion
        return Result<RowBatch>::success(RowBatch());
    }

    // Collect all left & right batches
    while (true) {
        auto lr = left->next();
        if (!lr) {
            return lr; // propagate error
        }
        auto lb = std::move(lr.value());
        if (lb.getRowCount() == 0) {
            break;
        }
        left_buffer_.addRows(std::move(lb));
    }
    while (true) {
        auto rr = right->next();
        if (!rr) {
            return rr;
        }
        auto rb = std::move(rr.value());
        if (rb.getRowCount() == 0) {
            break;
        }
        right_buffer_.addRows(std::move(rb));
    }

    // Perform a simple CPU merge inner equi-join on the first column (key)
    // Assumption: both `left_buffer_` and `right_buffer_` are sorted ascending by column 0.
    if (left_buffer_.getColumnCount() < 2 || right_buffer_.getColumnCount() < 2) {
        return Result<RowBatch>::failure("Input batches must expose key and $_rowid columns at indices 0 and 1");
    }
    auto joined_batch = RowBatch::sortMergeJoinBatches(left_buffer_, 0, right_buffer_, 0);
    joined_ = true;
    // TODO: build batches of appropriate size for output
    return Result<RowBatch>::success(std::move(joined_batch));
}

} // namespace velodb
