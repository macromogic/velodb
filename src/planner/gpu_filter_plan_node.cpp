#include "planner/gpu_filter_plan_node.hpp"

#include "catalog/execution_context.hpp"
#include "operator/gpu_filter_operator.hpp"

namespace velodb {

GpuFilterPlanNode::GpuFilterPlanNode(Schema output_schema, std::unique_ptr<AbstractExpression> predicate)
    : AbstractPlanNode(PlanType::GPU_FILTER, std::move(output_schema))
    , predicate_(std::move(predicate))
{
}

std::unique_ptr<AbstractOperator> GpuFilterPlanNode::createOperator(ExecutionContext& context) const
{
    VELODB_ASSERT_MSG(children_.size() == 1, "GpuFilterPlanNode must have exactly one child");
    auto child_operator = children_[0]->createOperator(context);
    return std::make_unique<GpuFilterOperator>(context,
                                               output_schema_.clone(),
                                               std::move(child_operator),
                                               predicate_->cloneUnique());
}

std::string GpuFilterPlanNode::toString() const
{
    return fmt::format("GpuFilter({})", predicate_->toString());
}

} // namespace velodb
