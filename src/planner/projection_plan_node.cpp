#include "planner/projection_plan_node.hpp"

#include "catalog/execution_context.hpp"
#include "common/exception.hpp"
#include "operator/projection_operator.hpp"

#include <fmt/ranges.h>

namespace velodb {

// ProjectionPlanNode implementation
ProjectionPlanNode::ProjectionPlanNode(Schema input_schema,
                                       Schema output_schema,
                                       std::vector<std::unique_ptr<AbstractExpression>> expressions)
    : AbstractPlanNode(PlanType::PROJECTION, std::move(output_schema))
    , input_schema_(std::move(input_schema))
    , expressions_(std::move(expressions))
{
}

std::unique_ptr<AbstractOperator> ProjectionPlanNode::createOperator(ExecutionContext& context) const
{
    // Create child operator
    VELODB_ASSERT_MSG(children_.size() == 1, "ProjectionPlanNode must have exactly one child");

    auto child_operator = children_[0]->createOperator(context);

    return std::make_unique<ProjectionOperator>(context,
                                                input_schema_.clone(),
                                                output_schema_.clone(),
                                                std::move(child_operator),
                                                std::move(expressions_));
}

std::string ProjectionPlanNode::toString() const
{
    if (expressions_.empty()) {
        return "Projection(*)";
    } else {
        return fmt::format("Projection({})", fmt::join(expressions_, ", "));
    }
}

} // namespace velodb
