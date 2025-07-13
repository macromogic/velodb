#include "planner/projection_plan_node.hpp"
#include <stdexcept>

namespace velodb {

// ProjectionPlanNode implementation
ProjectionPlanNode::ProjectionPlanNode(std::unique_ptr<Schema> output_schema,
    std::vector<std::unique_ptr<AbstractExpression>> expressions)
    : AbstractPlanNode(PlanType::PROJECTION, std::move(output_schema))
    , expressions_(std::move(expressions))
{
}

std::unique_ptr<AbstractOperator> ProjectionPlanNode::createOperator([[maybe_unused]] ExecutionContext* context) const
{
    // TODO: Implement projection operator creation
    throw std::runtime_error("ProjectionPlanNode::createOperator not implemented");
}

std::string ProjectionPlanNode::toString() const
{
    return "Projection(...)";
}

} // namespace velodb
