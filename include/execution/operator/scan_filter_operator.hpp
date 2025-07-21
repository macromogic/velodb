#pragma once

#include "execution/operator/abstract_operator.hpp"
#include "catalog/table.hpp"
#include "execution/expression.hpp"
#include <memory>

namespace velodb {

// Forward declarations
class TableIterator;

// Scan with filter operator
class ScanFilterOperator : public AbstractOperator {
public:
    explicit ScanFilterOperator(const TableBase& table, const std::unique_ptr<AbstractExpression>& predicate);
    ~ScanFilterOperator() override = default;

    // Main execution interface
    std::unique_ptr<QueryResult> execute() override;

    void init() override;
    bool nextRowId(RowId* row_id) override;
    void reset() override;
    
    // Access to the source table for materialization
    [[nodiscard]] const TableBase& getTable() const { return table_; }
    
    [[nodiscard]] std::string toString() const;

private:
    const TableBase& table_;
    const std::unique_ptr<AbstractExpression>& predicate_;
    std::unique_ptr<TableIterator> iterator_;
    bool initialized_;
};

} // namespace velodb
