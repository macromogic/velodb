#pragma once

#include "catalog/table.hpp"
#include "expression/expression.hpp"
#include "operator/abstract_operator.hpp"
#include <memory>

namespace velodb {

// Forward declarations
class TableIterator;

// Scan with filter operator
class ScanFilterOperator : public AbstractOperator {
public:
    explicit ScanFilterOperator(Catalog& catalog, const TableBase& table, const std::unique_ptr<AbstractExpression>& predicate);
    ~ScanFilterOperator() override = default;

    // Main execution interface
    View execute() override;

    void init() override;
    bool nextRowId(RowId* row_id) override;
    void reset() override;

    // Access to the source table for materialization
    const TableBase& getTable() const { return table_; }

    std::string toString() const;

private:
    const TableBase& table_;
    const std::unique_ptr<AbstractExpression>& predicate_;
};

} // namespace velodb
