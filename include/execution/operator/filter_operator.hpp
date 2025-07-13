#pragma once

#include "execution/operator/abstract_operator.hpp"
#include <memory>

namespace velodb {

// Forward declarations
class AbstractExpression;

// Filter operator
class FilterOperator : public AbstractOperator {
public:
    FilterOperator(std::unique_ptr<AbstractOperator> child,
        std::unique_ptr<AbstractExpression> predicate);
    ~FilterOperator() override = default;

    void init() override;
    void reset() override;

    // Late materialization interface
    bool nextRowId(RowId* row_id) override;
    void materializeRowIds(const std::vector<RowId>& row_ids,
        const std::vector<size_t>& column_indices,
        std::vector<Tuple>* tuples) override;

private:
    std::unique_ptr<AbstractOperator> child_;
    std::unique_ptr<AbstractExpression> predicate_;
};

} // namespace velodb
