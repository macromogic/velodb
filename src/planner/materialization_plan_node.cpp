#include "planner/materialization_plan_node.hpp"

#include "operator/materialization_operator.hpp"

namespace velodb {

std::unique_ptr<AbstractOperator> MaterializationPlanNode::createOperator(ExecutionContext& context) const
{
    VELODB_ASSERT_MSG(children_.size() == 1, "MaterializationPlanNode must have exactly one child");
    auto child_op = children_[0]->createOperator(context);
    return std::make_unique<MaterializationOperator>(context, output_schema_.clone(), std::move(child_op));
}

} // namespace velodb
