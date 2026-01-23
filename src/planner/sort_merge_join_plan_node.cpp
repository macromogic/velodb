#include "planner/sort_merge_join_plan_node.hpp"

#include "common/exception.hpp"

#include <stdexcept>

namespace velodb {

// SortMergeJoinPlanNode implementation
SortMergeJoinPlanNode::SortMergeJoinPlanNode(Schema output_schema,
                                             std::unique_ptr<AbstractExpression> left_key_expr,
                                             std::unique_ptr<AbstractExpression> right_key_expr,
                                             JoinType join_type)
    : AbstractPlanNode(PlanType::sort_merge_join, std::move(output_schema))
    , left_key_expr_(std::move(left_key_expr))
    , right_key_expr_(std::move(right_key_expr))
    , join_type_(join_type)
{
}

std::unique_ptr<AbstractOperator> SortMergeJoinPlanNode::createOperator(ExecutionContext& context) const
{
    VELODB_ASSERT_MSG(children_.size() == 2, "SortMergeJoinPlanNode must have exactly two children");

    auto left_operator = children_[0]->createOperator(context);
    auto right_operator = children_[1]->createOperator(context);

    return std::make_unique<SortMergeJoinOperator>(context,
                                                   output_schema_.clone(),
                                                   std::move(left_operator),
                                                   std::move(right_operator),
                                                   left_key_expr_->cloneUnique(),
                                                   right_key_expr_->cloneUnique(),
                                                   join_type_);
}

std::string SortMergeJoinPlanNode::toString() const
{
    return "SortMergeJoin(...)";
}

} // namespace velodb
