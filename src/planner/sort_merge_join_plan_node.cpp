#include "planner/sort_merge_join_plan_node.hpp"

#include "common/exception.hpp"

#include <stdexcept>

namespace velodb {

SortMergeJoinPlanNode::SortMergeJoinPlanNode(Schema output_schema,
                                             std::unique_ptr<AbstractPlanNode> left,
                                             std::unique_ptr<AbstractPlanNode> right,
                                             std::pair<size_t, size_t> join_key_indices,
                                             std::vector<const Table*> left_source_tables,
                                             std::vector<const Table*> right_source_tables,
                                             JoinType join_type)
    : AbstractPlanNode(PlanType::SORT_MERGE_JOIN, std::move(output_schema))
    , join_key_indices_(join_key_indices)
    , left_source_tables_(std::move(left_source_tables))
    , right_source_tables_(std::move(right_source_tables))
    , join_type_(join_type)
{
    addChild(std::move(left));
    addChild(std::move(right));
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
                                                   join_key_indices_,
                                                   left_source_tables_,
                                                   right_source_tables_,
                                                   join_type_);
}

std::string SortMergeJoinPlanNode::toString() const
{
    return "SortMergeJoin(...)";
}

} // namespace velodb
