#pragma once

#include "operator/abstract_operator.hpp"
#include "operator/join_type.hpp"
#include <memory>
#include <vector>

namespace velodb {

// Forward declarations
class AbstractExpression;

// Nested loop join operator
class NestedLoopJoinOperator : public AbstractOperator {
public:
    NestedLoopJoinOperator(std::unique_ptr<AbstractOperator> left_child,
        std::unique_ptr<AbstractOperator> right_child,
        std::unique_ptr<AbstractExpression> join_predicate,
        JoinType join_type = JoinType::INNER);
    ~NestedLoopJoinOperator() override = default;

    View execute() override;

private:
    std::unique_ptr<AbstractOperator> left_child_;
    std::unique_ptr<AbstractOperator> right_child_;
    std::unique_ptr<AbstractExpression> join_predicate_;
    JoinType join_type_;

    // TODO: Add state for join iteration with late materialization
    std::vector<RowId> left_row_ids_;
    std::vector<RowId> right_row_ids_;
    size_t current_left_index_ { 0 };
    size_t current_right_index_ { 0 };
    bool initialized_ { false };
};

} // namespace velodb
