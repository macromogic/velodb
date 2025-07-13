#pragma once

#include "execution/operator/abstract_operator.hpp"
#include "catalog/table.hpp"
#include <memory>

namespace velodb {

// Forward declarations
class AbstractExpression;

// Sequential scan operator
class SeqScanOperator : public AbstractOperator {
public:
    explicit SeqScanOperator(TableBase& table,
        std::unique_ptr<AbstractExpression> predicate = nullptr);
    ~SeqScanOperator() override = default;

    void init() override;
    void reset() override;

    // Late materialization interface
    bool nextRowId(RowId* row_id) override;
    void materializeRowIds(const std::vector<RowId>& row_ids,
        const std::vector<size_t>& column_indices,
        std::vector<Tuple>* tuples) override;

private:
    TableBase& table_;
    std::unique_ptr<AbstractExpression> predicate_;
    std::unique_ptr<TableIterator> iterator_;
    bool initialized_ { false };
};

} // namespace velodb
