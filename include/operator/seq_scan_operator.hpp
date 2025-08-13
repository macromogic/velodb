#pragma once

#include "catalog/table.hpp"
#include "expression/expression.hpp"
#include "operator/abstract_operator.hpp"

#include <memory>

namespace velodb {

// Forward declarations
class TableIterator;

// Scan with filter operator
class ScanFilterOperator : public UnaryOperator {
public:
    explicit ScanFilterOperator(ExecutionContext& context,
                                const TableBase& table,
                                const std::unique_ptr<AbstractExpression>& predicate);
    ~ScanFilterOperator() override = default;

    Result<View> next() override;

    // Access to the source table for materialization
    const TableBase& getTable() const { return table_; }

    std::string toString() const;

private:
    const TableBase& table_;
    const std::unique_ptr<AbstractExpression>& predicate_;
    ValueColumn& rowids_; // Temporary column for row IDs
    ValueColumn& masks_; // Temporary column for mask results
    size_t current_row_id_; // Current row ID for iteration
    TableIterator iterator_; // Iterator for the table
};

} // namespace velodb
