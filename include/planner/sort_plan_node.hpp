#pragma once

#include "planner/abstract_plan_node.hpp"
#include "execution/expression.hpp"
#include <memory>
#include <string>
#include <vector>

namespace velodb {

// Sort plan node
class SortPlanNode : public AbstractPlanNode {
public:
    SortPlanNode(std::unique_ptr<Schema> output_schema,
        std::vector<std::unique_ptr<AbstractExpression>> sort_expressions,
        std::vector<bool> ascending_flags);
    ~SortPlanNode() override = default;

    std::unique_ptr<AbstractOperator> createOperator(ExecutionContext* context) const override;
    [[nodiscard]] std::string toString() const override;

    [[nodiscard]] const std::vector<std::unique_ptr<AbstractExpression>>& getSortExpressions() const { return sort_expressions_; }
    [[nodiscard]] const std::vector<bool>& getAscendingFlags() const { return ascending_flags_; }

private:
    std::vector<std::unique_ptr<AbstractExpression>> sort_expressions_;
    std::vector<bool> ascending_flags_;
};

} // namespace velodb
