#include "planner/filter_plan_node.hpp"
#include <stdexcept>

namespace velodb {

// FilterPlanNode implementation
FilterPlanNode::FilterPlanNode(std::unique_ptr<Schema> output_schema,
    std::unique_ptr<AbstractExpression> predicate)
    : AbstractPlanNode(PlanType::FILTER, std::move(output_schema))
    , predicate_(std::move(predicate))
{
}

std::unique_ptr<AbstractOperator> FilterPlanNode::createOperator([[maybe_unused]] ExecutionContext* context) const
{
    // TODO: Implement filter operator creation
    throw std::runtime_error("FilterPlanNode::CreateOperator not implemented");
}

std::string FilterPlanNode::toString() const
{
    return "Filter(" + predicate_->toString() + ")";
}

} // namespace velodb
