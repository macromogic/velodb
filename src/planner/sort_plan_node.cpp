#include "planner/sort_plan_node.hpp"

#include "common/exception.hpp"

#include <stdexcept>

namespace velodb {

// SortPlanNode implementation
SortPlanNode::SortPlanNode(Schema output_schema,
                           std::vector<std::unique_ptr<AbstractExpression>> sort_expressions,
                           std::vector<bool> ascending_flags)
    : AbstractPlanNode(PlanType::SORT, std::move(output_schema))
    , sort_expressions_(std::move(sort_expressions))
    , ascending_flags_(std::move(ascending_flags))
{
}

std::unique_ptr<AbstractOperator> SortPlanNode::createOperator([[maybe_unused]] ExecutionContext& context) const
{
    VELODB_ASSERT_MSG(children_.size() == 1, "SortPlanNode must have exactly one child");

    auto child_operator = children_[0]->createOperator(context);
    auto sort_expressions = std::vector<std::unique_ptr<AbstractExpression>>();
    sort_expressions.reserve(sort_expressions_.size());
    for (const auto& expr : sort_expressions_) {
        sort_expressions.push_back(expr->cloneUnique());
    }
    return std::make_unique<SortOperator>(context,
                                          output_schema_.clone(),
                                          std::move(child_operator),
                                          std::move(sort_expressions),
                                          std::move(ascending_flags_));
}

std::string SortPlanNode::toString() const
{
    return "Sort(...)";
}

} // namespace velodb
