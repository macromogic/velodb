#pragma once

#include "execution/operator/abstract_operator.hpp"
#include "execution/operator/join_type.hpp"
#include <memory>
#include <vector>

namespace velodb {

// Forward declarations
class AbstractExpression;

// Merge sort join operator
class MergeSortJoinOperator : public AbstractOperator {
public:
    MergeSortJoinOperator(std::unique_ptr<AbstractOperator> left_child,
        std::unique_ptr<AbstractOperator> right_child,
        std::unique_ptr<AbstractExpression> left_key_expr,
        std::unique_ptr<AbstractExpression> right_key_expr,
        JoinType join_type = JoinType::INNER);
    ~MergeSortJoinOperator() override = default;

    void init() override;
    void reset() override;

    // Late materialization interface
    bool nextRowId(RowId* row_id) override;
    void materializeRowIds(const std::vector<RowId>& row_ids,
        const std::vector<size_t>& column_indices,
        std::vector<Tuple>* tuples) override;

private:
    std::unique_ptr<AbstractOperator> left_child_;
    std::unique_ptr<AbstractOperator> right_child_;
    std::unique_ptr<AbstractExpression> left_key_expr_;
    std::unique_ptr<AbstractExpression> right_key_expr_;
    JoinType join_type_;

    // TODO: Add state for merge sort join with late materialization
    std::vector<RowId> result_row_ids_;
    size_t current_result_index_ { 0 };
    bool inputs_sorted_ { false };
};

} // namespace velodb
