#include "operator/merge_sort_join_operator.hpp"
#include "expression/expression.hpp"
#include <stdexcept>

namespace velodb {

// MergeSortJoinOperator implementation
MergeSortJoinOperator::MergeSortJoinOperator(Catalog& catalog,
    std::unique_ptr<Schema> output_schema,
    std::unique_ptr<AbstractOperator> left_child,
    std::unique_ptr<AbstractOperator> right_child,
    std::unique_ptr<AbstractExpression> left_key_expr,
    std::unique_ptr<AbstractExpression> right_key_expr,
    JoinType join_type)
    : BinaryOperator(catalog, std::move(output_schema), std::move(left_child), std::move(right_child))
    , left_key_expr_(std::move(left_key_expr))
    , right_key_expr_(std::move(right_key_expr))
    , join_type_(join_type)
{
}

Result<View> MergeSortJoinOperator::execute() const
{
    // TODO: Implement merge sort join logic
    return Result<View>::failure("MergeSortJoinOperator::execute not implemented yet");
}

} // namespace velodb
