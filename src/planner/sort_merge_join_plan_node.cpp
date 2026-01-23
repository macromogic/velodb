#include "planner/sort_merge_join_plan_node.hpp"

#include "common/exception.hpp"

#include <stdexcept>

namespace velodb {

SortMergeJoinPlanNode::SortMergeJoinPlanNode(Schema output_schema,
                                             const Table& left_table,
                                             const Table& right_table,
                                             JoinType join_type)
    : AbstractPlanNode(PlanType::SORT_MERGE_JOIN, std::move(output_schema))
    , left_table_(left_table)
    , right_table_(right_table)
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
                                                   left_table_.get(),
                                                   right_table_.get(),
                                                   join_type_);
}

std::string SortMergeJoinPlanNode::toString() const
{
    return "SortMergeJoin(...)";
}

} // namespace velodb
