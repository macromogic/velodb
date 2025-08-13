#pragma once

#include "operator/abstract_operator.hpp"
#include "operator/join_type.hpp"

#include <memory>
#include <vector>

namespace velodb {

// Forward declarations
class AbstractExpression;

// Nested loop join operator
class NestedLoopJoinOperator : public BinaryOperator {
public:
    NestedLoopJoinOperator(ExecutionContext& context,
                           std::unique_ptr<Schema> output_schema,
                           std::unique_ptr<AbstractOperator> left_child,
                           std::unique_ptr<AbstractOperator> right_child,
                           std::unique_ptr<AbstractExpression> join_predicate,
                           JoinType join_type = JoinType::INNER);
    ~NestedLoopJoinOperator() override = default;

    Result<View> next() override;

private:
    std::unique_ptr<AbstractExpression> join_predicate_;
    JoinType join_type_;

    // TODO: Add state for join iteration with late materialization
    std::vector<size_t> left_row_ids_;
    std::vector<size_t> right_row_ids_;
    size_t current_left_index_ { 0 };
    size_t current_right_index_ { 0 };
    bool initialized_ { false };
};

} // namespace velodb
