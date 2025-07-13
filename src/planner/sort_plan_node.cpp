#include "planner/sort_plan_node.hpp"
#include <stdexcept>
#include <sstream>

namespace velodb {

// SortPlanNode implementation
SortPlanNode::SortPlanNode(std::unique_ptr<Schema> output_schema,
    std::vector<std::unique_ptr<AbstractExpression>> sort_expressions,
    std::vector<bool> ascending_flags)
    : AbstractPlanNode(PlanType::SORT, std::move(output_schema))
    , sort_expressions_(std::move(sort_expressions))
    , ascending_flags_(std::move(ascending_flags))
{
}

std::unique_ptr<AbstractOperator> SortPlanNode::createOperator([[maybe_unused]] ExecutionContext* context) const
{
    // TODO: Implement sort operator creation
    throw std::runtime_error("SortPlanNode::createOperator not implemented");
}

std::string SortPlanNode::toString() const
{
    return "Sort(...)";
}

} // namespace velodb
