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
    explicit SeqScanOperator(const TableBase& table,
        const std::unique_ptr<AbstractExpression>& predicate);
    ~SeqScanOperator() override = default;

    void init() override;
    void reset() override;

    // Late materialization interface
    bool nextRowId(RowId* row_id) override;
    
    // Access to the source table for materialization
    [[nodiscard]] const TableBase& getTable() const { return table_; }

private:
    const TableBase& table_;
    const std::unique_ptr<AbstractExpression>& predicate_;
    std::unique_ptr<TableIterator> iterator_;
    bool initialized_ { false };
};

} // namespace velodb
