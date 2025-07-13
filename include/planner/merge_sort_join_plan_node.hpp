#pragma once

#include "planner/abstract_plan_node.hpp"
#include "execution/expression.hpp"
#include "execution/operator.hpp"
#include <memory>
#include <string>

namespace velodb {

// Merge sort join plan node
class MergeSortJoinPlanNode : public AbstractPlanNode {
public:
    MergeSortJoinPlanNode(std::unique_ptr<Schema> output_schema,
        std::unique_ptr<AbstractExpression> left_key_expr,
        std::unique_ptr<AbstractExpression> right_key_expr,
        JoinType join_type = JoinType::INNER);
    ~MergeSortJoinPlanNode() override = default;

    std::unique_ptr<AbstractOperator> createOperator(ExecutionContext* context) const override;
    [[nodiscard]] std::string toString() const override;

    [[nodiscard]] const AbstractExpression* getLeftKeyExpression() const { return left_key_expr_.get(); }
    [[nodiscard]] const AbstractExpression* getRightKeyExpression() const { return right_key_expr_.get(); }
    [[nodiscard]] JoinType getJoinType() const { return join_type_; }

private:
    std::unique_ptr<AbstractExpression> left_key_expr_;
    std::unique_ptr<AbstractExpression> right_key_expr_;
    JoinType join_type_;
};

} // namespace velodb
