#include "planner/sort_plan_node.hpp"

#include "common/exception.hpp"

#include <fmt/ranges.h>

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
    std::vector<std::string> sort_strs;
    for (size_t i = 0; i < order_indices_.size(); ++i) {
        auto& sort_col = output_schema_.getColumnInfo(order_indices_[i]);
        sort_strs.push_back(fmt::format("{} {}", sort_col.getName(), ascending_flags_[i] ? "ASC" : "DESC"));
    }
    return fmt::format("Sort({})", fmt::join(sort_strs, ", "));
}

} // namespace velodb
