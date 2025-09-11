#include "planner/abstract_plan_node.hpp"

namespace velodb {

// AbstractPlanNode implementation
AbstractPlanNode::AbstractPlanNode(PlanType type, Schema output_schema)
    : type_(type)
    , output_schema_(std::move(output_schema))
{
}

void AbstractPlanNode::addChild(std::unique_ptr<AbstractPlanNode> child)
{
    children_.push_back(std::move(child));
}

} // namespace velodb
