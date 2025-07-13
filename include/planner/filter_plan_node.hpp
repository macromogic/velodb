#pragma once

#include "planner/abstract_plan_node.hpp"
#include "execution/expression.hpp"
#include <memory>
#include <string>

namespace velodb {

// Filter plan node
class FilterPlanNode : public AbstractPlanNode {
public:
    FilterPlanNode(std::unique_ptr<Schema> output_schema,
        std::unique_ptr<AbstractExpression> predicate);
    ~FilterPlanNode() override = default;

    std::unique_ptr<AbstractOperator> createOperator(ExecutionContext* context) const override;
    [[nodiscard]] std::string toString() const override;

    [[nodiscard]] const AbstractExpression& getPredicate() const { return *predicate_; }

private:
    std::unique_ptr<AbstractExpression> predicate_;
};

} // namespace velodb
