#include "planner/projection_plan_node.hpp"

#include "catalog/execution_context.hpp"
#include "common/exception.hpp"
#include "common/fmt.hpp"
#include "operator/projection_operator.hpp"

#include <fmt/ranges.h>

#include <stdexcept>

namespace velodb {

// ProjectionPlanNode implementation
ProjectionPlanNode::ProjectionPlanNode(std::unique_ptr<Schema> output_schema,
                                       std::vector<std::unique_ptr<AbstractExpression>> expressions)
    : AbstractPlanNode(PlanType::PROJECTION, std::move(output_schema))
    , expressions_(std::move(expressions))
{
}

std::unique_ptr<AbstractOperator> ProjectionPlanNode::createOperator(ExecutionContext& context) const
{
    // Create child operator
    if (children_.size() != 1) {
        VELODB_THROW(ExecutionError, "ProjectionPlanNode must have exactly one child");
    }

    auto child_operator = children_[0]->createOperator(context);

    return std::make_unique<ProjectionOperator>(context,
                                                output_schema_->cloneUnique(),
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
