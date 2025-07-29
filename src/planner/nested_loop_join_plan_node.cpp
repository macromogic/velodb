#include "planner/nested_loop_join_plan_node.hpp"
#include "common/exception.hpp"
#include <stdexcept>

namespace velodb {

// NestedLoopJoinPlanNode implementation
NestedLoopJoinPlanNode::NestedLoopJoinPlanNode(std::unique_ptr<Schema> output_schema,
    std::unique_ptr<AbstractExpression> join_predicate,
    JoinType join_type)
    : AbstractPlanNode(PlanType::NESTED_LOOP_JOIN, std::move(output_schema))
    , join_predicate_(std::move(join_predicate))
    , join_type_(join_type)
{
}

std::unique_ptr<AbstractOperator> NestedLoopJoinPlanNode::createOperator([[maybe_unused]] ExecutionContext& context) const
{
    // TODO: Implement join operator creation
    VELODB_THROW(ExecutionError, "NestedLoopJoinPlanNode::createOperator not implemented");
}

std::string NestedLoopJoinPlanNode::toString() const
{
    return "NestedLoopJoin(...)";
}

} // namespace velodb
