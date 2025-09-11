#include "planner/merge_sort_join_plan_node.hpp"

#include "common/exception.hpp"

#include <stdexcept>

namespace velodb {

// MergeSortJoinPlanNode implementation
MergeSortJoinPlanNode::MergeSortJoinPlanNode(Schema output_schema,
                                             std::unique_ptr<AbstractExpression> left_key_expr,
                                             std::unique_ptr<AbstractExpression> right_key_expr,
                                             JoinType join_type)
    : AbstractPlanNode(PlanType::MERGE_SORT_JOIN, std::move(output_schema))
    , left_key_expr_(std::move(left_key_expr))
    , right_key_expr_(std::move(right_key_expr))
    , join_type_(join_type)
{
}

std::unique_ptr<AbstractOperator> MergeSortJoinPlanNode::createOperator(ExecutionContext& context) const
{
    VELODB_ASSERT_MSG(children_.size() == 2, "MergeSortJoinPlanNode must have exactly two children");

    auto left_operator = children_[0]->createOperator(context);
    auto right_operator = children_[1]->createOperator(context);

    return std::make_unique<MergeSortJoinOperator>(context,
                                                   output_schema_.clone(),
                                                   std::move(left_operator),
                                                   std::move(right_operator),
                                                   left_key_expr_->cloneUnique(),
                                                   right_key_expr_->cloneUnique(),
                                                   join_type_);
}

std::string MergeSortJoinPlanNode::toString() const
{
    return "MergeSortJoin(...)";
}

} // namespace velodb
