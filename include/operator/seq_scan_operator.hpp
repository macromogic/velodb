#pragma once

#include "catalog/table.hpp"
#include "expression/expression.hpp"
#include "operator/abstract_operator.hpp"

#include <memory>

namespace velodb {

// Forward declarations
class BatchIterator;

// Scan with filter operator
class SeqScanOperator : public UnaryOperator {
public:
    explicit SeqScanOperator(ExecutionContext& context,
                             const Table& table,
                             Schema output_schema,
                             const std::unique_ptr<AbstractExpression>& predicate);
    ~SeqScanOperator() override = default;

    Result<RowBatch> next() override;

    // Access to the source table for materialization
    const Table& getTable() const { return table_; }

    std::string toString() const;

private:
    const Table& table_;
    const std::unique_ptr<AbstractExpression>& predicate_;
    size_t current_row_id_;
};

} // namespace velodb
