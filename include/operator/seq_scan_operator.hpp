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

    Result<View> next() const override;

    // Access to the source table for materialization
    const TableBase& getTable() const { return table_; }

    std::string toString() const;

private:
    const TableBase& table_;
    const std::unique_ptr<AbstractExpression>& predicate_;
};

} // namespace velodb
