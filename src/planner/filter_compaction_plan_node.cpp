#include "planner/filter_compaction_plan_node.hpp"

#include "catalog/execution_context.hpp"
#include "operator/filter_compaction_operator.hpp"

namespace velodb {

FilterCompactionPlanNode::FilterCompactionPlanNode(Schema output_schema)
    : AbstractPlanNode(PlanType::COMPACTION, std::move(output_schema))
{
}

std::unique_ptr<AbstractOperator> FilterCompactionPlanNode::createOperator(ExecutionContext& context) const
{
    VELODB_ASSERT_MSG(children_.size() == 1, "FilterCompactionPlanNode must have exactly one child");
    auto child_operator = children_[0]->createOperator(context);
    return std::make_unique<FilterCompactionOperator>(context, output_schema_.clone(), std::move(child_operator));
}

std::string FilterCompactionPlanNode::toString() const
{
    return "FilterCompaction()";
}

} // namespace velodb
