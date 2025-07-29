#include "planner/compaction_plan_node.hpp"
#include "operator/compaction_operator.hpp"
#include "planner/execution_context.hpp"
#include <sstream>

namespace velodb {

CompactionPlanNode::CompactionPlanNode(std::unique_ptr<Schema> output_schema)
    : AbstractPlanNode(PlanType::COMPACTION, std::move(output_schema))
{
}

std::unique_ptr<AbstractOperator> CompactionPlanNode::createOperator(ExecutionContext& context) const
{
    if (children_.size() != 1) {
        VELODB_THROW(ExecutionError, "CompactionPlanNode must have exactly one child");
    }
    auto child_operator = children_[0]->createOperator(context);
    return std::make_unique<CompactionOperator>(context.getCatalog(), output_schema_->cloneUnique(), std::move(child_operator));
}

std::string CompactionPlanNode::toString() const
{
    return "Compaction()";
}

} // namespace velodb
