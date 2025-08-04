#include "planner/filter_compaction_plan_node.hpp"

#include "operator/filter_compaction_operator.hpp"
#include "planner/execution_context.hpp"

namespace velodb {

FilterCompactionPlanNode::FilterCompactionPlanNode(std::unique_ptr<Schema> output_schema)
    : AbstractPlanNode(PlanType::COMPACTION, std::move(output_schema))
{
}

std::unique_ptr<AbstractOperator> FilterCompactionPlanNode::createOperator(ExecutionContext& context) const
{
    if (children_.size() != 1) {
        VELODB_THROW(ExecutionError, "FilterCompactionPlanNode must have exactly one child");
    }
    auto child_operator = children_[0]->createOperator(context);
    return std::make_unique<FilterCompactionOperator>(context.getCatalog(),
                                                      output_schema_->cloneUnique(),
                                                      std::move(child_operator));
}

std::string FilterCompactionPlanNode::toString() const
{
    return "FilterCompaction()";
}

} // namespace velodb
