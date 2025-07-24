#pragma once

#include "execution/operator/abstract_operator.hpp"
#include "execution/operator/join_type.hpp"
#include <memory>
#include <vector>

namespace velodb {

// Forward declarations
class AbstractExpression;

// Hash join operator
class HashJoinOperator : public AbstractOperator {
public:
    HashJoinOperator(std::unique_ptr<AbstractOperator> left_child,
        std::unique_ptr<AbstractOperator> right_child,
        std::unique_ptr<AbstractExpression> left_key_expr,
        std::unique_ptr<AbstractExpression> right_key_expr,
        JoinType join_type = JoinType::INNER);
    ~HashJoinOperator() override = default;

    void init() override;
    void reset() override;

    // Late materialization interface
    bool nextRowId(RowId* row_id) override;

private:
    std::unique_ptr<AbstractOperator> left_child_;
    std::unique_ptr<AbstractOperator> right_child_;
    std::unique_ptr<AbstractExpression> left_key_expr_;
    std::unique_ptr<AbstractExpression> right_key_expr_;
    JoinType join_type_;

    // TODO: Add state for hash join with late materialization
    std::vector<RowId> result_row_ids_;
    size_t current_result_index_ { 0 };
    bool hash_table_built_ { false };
};

} // namespace velodb
