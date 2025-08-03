#pragma once

#include "expression/expression.hpp"
#include "operator/operator.hpp"
#include "planner/abstract_plan_node.hpp"

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

    std::unique_ptr<AbstractOperator> createOperator(ExecutionContext& context) const override;
    std::string toString() const override;

    const AbstractExpression* getLeftKeyExpression() const { return left_key_expr_.get(); }
    const AbstractExpression* getRightKeyExpression() const { return right_key_expr_.get(); }
    JoinType getJoinType() const { return join_type_; }

private:
    std::unique_ptr<AbstractExpression> left_key_expr_;
    std::unique_ptr<AbstractExpression> right_key_expr_;
    JoinType join_type_;
};

} // namespace velodb
