#include "planner/projection_plan_node.hpp"
#include "execution/operator/projection_operator.hpp"
#include <stdexcept>

namespace velodb {

// ProjectionPlanNode implementation
ProjectionPlanNode::ProjectionPlanNode(std::unique_ptr<Schema> output_schema,
    std::vector<std::unique_ptr<AbstractExpression>> expressions)
    : AbstractPlanNode(PlanType::PROJECTION, std::move(output_schema))
    , expressions_(std::move(expressions))
{
}

std::unique_ptr<AbstractOperator> ProjectionPlanNode::createOperator([[maybe_unused]] ExecutionContext* context) const
{
    // Create child operator
    if (children_.size() != 1) {
        throw std::runtime_error("ProjectionPlanNode must have exactly one child");
    }

    auto child_operator = children_[0]->createOperator(context);

    // Move expressions to the operator (expressions_ is mutable)
    auto expressions_copy = std::move(expressions_);

    return std::make_unique<ProjectionOperator>(output_schema_->clone(), std::move(child_operator), std::move(expressions_copy));
}

std::string ProjectionPlanNode::toString() const
{
    std::string result = "Projection(";

    if (expressions_.empty()) {
        result += "*";
    } else {
        for (size_t i = 0; i < expressions_.size(); ++i) {
            if (i > 0)
                result += ", ";
            result += expressions_[i]->toString();
        }
    }

    result += ")";
    return result;
}

} // namespace velodb
