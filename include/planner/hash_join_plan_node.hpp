#pragma once

#include "expression/expression.hpp"
#include "operator/operator.hpp"
#include "planner/abstract_plan_node.hpp"

#include <memory>
#include <string>

namespace velodb {

class HashJoinPlanNode : public AbstractPlanNode {
public:
    HashJoinPlanNode(Schema output_schema,
                     std::unique_ptr<AbstractPlanNode> left,
                     std::unique_ptr<AbstractPlanNode> right,
                     std::pair<size_t, size_t> join_key_indices,
                     std::pair<ssize_t, ssize_t> mask_indices,
                     std::vector<const Table*> left_source_tables,
                     std::vector<const Table*> right_source_tables,
                     JoinType join_type = JoinType::INNER);
    ~HashJoinPlanNode() override = default;

    std::unique_ptr<AbstractOperator> createOperator(ExecutionContext& context) const override;
    std::string toString() const override;

    JoinType getJoinType() const { return join_type_; }

private:
    std::pair<size_t, size_t> join_key_indices_;
    std::pair<ssize_t, ssize_t> mask_indices_;
    std::vector<const Table*> left_source_tables_;
    std::vector<const Table*> right_source_tables_;
    JoinType join_type_;
};

} // namespace velodb
