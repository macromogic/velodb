#include "common/traced_exception.hpp"
#include "planner/hash_join_plan_node.hpp"
#include <stdexcept>

namespace velodb {

// HashJoinPlanNode implementation
HashJoinPlanNode::HashJoinPlanNode(std::unique_ptr<Schema> output_schema,
    std::unique_ptr<AbstractExpression> left_key_expr,
    std::unique_ptr<AbstractExpression> right_key_expr,
    JoinType join_type)
    : AbstractPlanNode(PlanType::HASH_JOIN, std::move(output_schema))
    , left_key_expr_(std::move(left_key_expr))
    , right_key_expr_(std::move(right_key_expr))
    , join_type_(join_type)
{
}

std::unique_ptr<AbstractOperator> HashJoinPlanNode::createOperator([[maybe_unused]] ExecutionContext* context) const
{
    // TODO: Implement hash join operator creation
    VELODB_THROW(ExecutionError, "HashJoinPlanNode::createOperator not implemented");
}

std::string HashJoinPlanNode::toString() const
{
    return "HashJoin(...)";
}

} // namespace velodb
