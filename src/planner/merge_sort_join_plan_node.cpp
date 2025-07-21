#include "common/traced_exception.hpp"
#include "planner/merge_sort_join_plan_node.hpp"
#include <stdexcept>

namespace velodb {

// MergeSortJoinPlanNode implementation
MergeSortJoinPlanNode::MergeSortJoinPlanNode(std::unique_ptr<Schema> output_schema,
    std::unique_ptr<AbstractExpression> left_key_expr,
    std::unique_ptr<AbstractExpression> right_key_expr,
    JoinType join_type)
    : AbstractPlanNode(PlanType::MERGE_SORT_JOIN, std::move(output_schema))
    , left_key_expr_(std::move(left_key_expr))
    , right_key_expr_(std::move(right_key_expr))
    , join_type_(join_type)
{
}

std::unique_ptr<AbstractOperator> MergeSortJoinPlanNode::createOperator([[maybe_unused]] ExecutionContext* context) const
{
    // TODO: Implement merge sort join operator creation
    VELODB_THROW(ExecutionError, "MergeSortJoinPlanNode::createOperator not implemented");
}

std::string MergeSortJoinPlanNode::toString() const
{
    return "MergeSortJoin(...)";
}

} // namespace velodb
