#pragma once

#include "operator/abstract_operator.hpp"

namespace velodb {

// Materialization operator: given rowid pairs (left_rowid,right_rowid) fetches columns from original tables.
// Current simplified implementation: just forwards input (placeholder) until full table access layer ready.
class MaterializationOperator : public UnaryOperator {
public:
    MaterializationOperator(ExecutionContext& context, Schema output_schema, std::unique_ptr<AbstractOperator> child)
        : UnaryOperator(context, std::move(output_schema), std::move(child))
        , produced_(false)
    {
    }
    ~MaterializationOperator() override = default;

    Result<RowBatch> next() override;

private:
    bool produced_;
};

} // namespace velodb
