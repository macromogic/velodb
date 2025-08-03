#pragma once

#include "expression/expression.hpp"
#include "operator/operator.hpp"
#include "planner/abstract_plan_node.hpp"

#include <memory>
#include <string>

namespace velodb {

// Nested loop join plan node
class NestedLoopJoinPlanNode : public AbstractPlanNode {
public:
    NestedLoopJoinPlanNode(std::unique_ptr<Schema> output_schema,
                           std::unique_ptr<AbstractExpression> join_predicate,
                           JoinType join_type = JoinType::INNER);
    ~NestedLoopJoinPlanNode() override = default;

    std::unique_ptr<AbstractOperator> createOperator(ExecutionContext& context) const override;
    std::string toString() const override;

    const AbstractExpression* getJoinPredicate() const { return join_predicate_.get(); }
    JoinType getJoinType() const { return join_type_; }

private:
    std::unique_ptr<AbstractExpression> join_predicate_;
    JoinType join_type_;
};

} // namespace velodb
