#include "operator/sort_operator.hpp"

#include "catalog/row_batch.hpp"
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
    , buffer_(RowBatch::createBuffered(output_schema_, MAX_BATCH_SIZE * 2, DataLocation::CUDA))
{
}

Result<RowBatch> SortOperator::next()
{
    if (!sorted_) {
        auto* child = getChild();
        if (!child) {
            return Result<RowBatch>::failure("FilterCompactionOperator requires a child operator");
        }
        if (!output_schema_.hasColumn("$_rowid")) {
            return Result<RowBatch>::failure("SortOperator requires $_rowid column in output schema");
        }
        auto rowid_index = output_schema_.getColumnIndex("$_rowid");
        bool reverse = false;
        while (true) {
            auto child_result = child->next();
            if (!child_result) {
                return child_result;
            }
            auto& batch = child_result.value();
            if (batch.getRowCount() == 0) {
                break;
            }
            batch.sort(order_indices_, ascending_flags_, rowid_index, 1, reverse);
            buffer_.addRows(batch);
            reverse = !reverse;
        }
        buffer_.sort(order_indices_, ascending_flags_, rowid_index, MAX_BATCH_SIZE);
        sorted_ = true;
    }
    return Result<RowBatch>::success(buffer_.splitFront(MAX_BATCH_SIZE));
}

} // namespace velodb
