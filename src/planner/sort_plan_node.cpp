#include "planner/sort_plan_node.hpp"

#include "common/exception.hpp"

#include <stdexcept>

namespace velodb {

// SortPlanNode implementation
SortPlanNode::SortPlanNode(Schema output_schema, std::vector<size_t> order_indices, std::vector<bool> ascending_flags)
    : AbstractPlanNode(PlanType::SORT, std::move(output_schema))
    , order_indices_(std::move(order_indices))
    , ascending_flags_(std::move(ascending_flags))
{
}

std::unique_ptr<AbstractOperator> SortPlanNode::createOperator([[maybe_unused]] ExecutionContext& context) const
{
    VELODB_ASSERT_MSG(children_.size() == 1, "SortPlanNode must have exactly one child");

    auto child_operator = children_[0]->createOperator(context);
    return std::make_unique<SortOperator>(context,
                                          output_schema_.clone(),
                                          std::move(child_operator),
                                          std::move(order_indices_),
                                          std::move(ascending_flags_));
}

std::string SortPlanNode::toString() const
{
    return "Sort(...)";
}

} // namespace velodb
