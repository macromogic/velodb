#pragma once

#include "planner/abstract_plan_node.hpp"
#include "execution/expression.hpp"
#include <memory>
#include <string>
#include <vector>

namespace velodb {

// Projection plan node
class ProjectionPlanNode : public AbstractPlanNode {
public:
    ProjectionPlanNode(std::unique_ptr<Schema> output_schema,
        std::vector<std::unique_ptr<AbstractExpression>> expressions);
    ~ProjectionPlanNode() override = default;

    std::unique_ptr<AbstractOperator> createOperator(ExecutionContext* context) const override;
    [[nodiscard]] std::string toString() const override;

    [[nodiscard]] const std::vector<std::unique_ptr<AbstractExpression>>& getExpressions() const { return expressions_; }

private:
    mutable std::vector<std::unique_ptr<AbstractExpression>> expressions_;
};

} // namespace velodb
