#pragma once

#include "catalog/execution_context.hpp"
#include "catalog/row_batch.hpp"
#include "catalog/schema.hpp"
#include "common/copy_traits.hpp"
#include "common/profiler.hpp"
#include "common/result.hpp"

#include <memory>

namespace velodb {

// Forward declarations
class RowBatch;

// Abstract base class for all operators
class AbstractOperator : private NonCopyable {
public:
    explicit AbstractOperator(ExecutionContext& context, Schema output_schema);
    virtual ~AbstractOperator() = default;

    const Schema& getOutputSchema() const { return output_schema_; }

    virtual Result<RowBatch> next() = 0;
    virtual bool isUnary() const = 0;

protected:
    ExecutionContext& context_;
    Schema output_schema_;

    void setNumRowsForBatch(RowBatch& batch, size_t num_rows) { batch.setRowCount(num_rows); }
    std::vector<Column> extractColumnsFromBatch(RowBatch&& batch) { return std::move(batch).columns_; }
    RowBatch buildBatchFromColumns(std::vector<Column>&& columns) { return RowBatch(std::move(columns)); }
    RowBatch collectBatches(AbstractOperator& child);
};

class UnaryOperator : public AbstractOperator {
public:
    explicit UnaryOperator(ExecutionContext& context, Schema output_schema, std::unique_ptr<AbstractOperator> child);

    bool isUnary() const override { return true; }

protected:
    std::unique_ptr<AbstractOperator> child_; // Child operator
};

class BinaryOperator : public AbstractOperator {
public:
    BinaryOperator(ExecutionContext& context,
                   Schema output_schema,
                   std::unique_ptr<AbstractOperator> left_child,
                   std::unique_ptr<AbstractOperator> right_child);

    bool isUnary() const override { return false; }

protected:
    std::unique_ptr<AbstractOperator> left_child_;
    std::unique_ptr<AbstractOperator> right_child_;
};

} // namespace velodb
