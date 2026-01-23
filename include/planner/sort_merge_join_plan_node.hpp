#pragma once

#include "expression/expression.hpp"
#include "operator/operator.hpp"
#include "planner/abstract_plan_node.hpp"

#include <memory>
#include <string>

namespace velodb {

// Merge sort join plan node
class SortMergeJoinPlanNode : public AbstractPlanNode {
public:
    SortMergeJoinPlanNode(Schema output_schema,
                          const Table& left_table,
                          const Table& right_table,
                          JoinType join_type = JoinType::INNER);
    ~SortMergeJoinPlanNode() override = default;

    std::unique_ptr<AbstractOperator> createOperator(ExecutionContext& context) const override;
    std::string toString() const override;

    JoinType getJoinType() const { return join_type_; }

private:
    std::reference_wrapper<const Table> left_table_;
    std::reference_wrapper<const Table> right_table_;
    JoinType join_type_;
};

} // namespace velodb
