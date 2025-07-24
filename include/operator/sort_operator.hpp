#pragma once

#include "execution/operator/abstract_operator.hpp"
#include <memory>
#include <vector>

namespace velodb {

// Forward declarations
class AbstractExpression;

// Sort operator
class SortOperator : public AbstractOperator {
public:
    SortOperator(std::unique_ptr<AbstractOperator> child,
        std::vector<std::unique_ptr<AbstractExpression>> sort_expressions,
        std::vector<bool> ascending_flags);
    ~SortOperator() override = default;

    void init() override;
    void reset() override;

    // Late materialization interface
    bool nextRowId(RowId* row_id) override;

private:
    std::unique_ptr<AbstractOperator> child_;
    std::vector<std::unique_ptr<AbstractExpression>> sort_expressions_;
    std::vector<bool> ascending_flags_;

    // TODO: Add state for sort with late materialization
    std::vector<RowId> sorted_row_ids_;
    size_t current_index_ { 0 };
    bool sorted_ { false };
};

} // namespace velodb
