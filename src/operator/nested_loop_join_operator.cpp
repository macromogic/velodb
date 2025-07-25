#include "operator/nested_loop_join_operator.hpp"
#include "expression/expression.hpp"

namespace velodb {

// NestedLoopJoinOperator implementation
NestedLoopJoinOperator::NestedLoopJoinOperator(Catalog& catalog,
        std::unique_ptr<Schema> output_schema,
        std::unique_ptr<AbstractOperator> left_child,
        std::unique_ptr<AbstractOperator> right_child,
        std::unique_ptr<AbstractExpression> join_predicate,
        JoinType join_type)
    : BinaryOperator(catalog, std::move(output_schema), std::move(left_child), std::move(right_child))
    , join_predicate_(std::move(join_predicate))
    , join_type_(join_type)
{
}

Result<View> NestedLoopJoinOperator::execute() const
{
    // TODO: Implement nested loop join logic
    return Result<View>::failure("NestedLoopJoinOperator::execute not implemented yet");
}

} // namespace velodb
