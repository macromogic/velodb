#pragma once

#include "operator/abstract_operator.hpp"

namespace velodb {

class MaterializationOperator : public UnaryOperator {
    struct ColumnMapping {
        size_t rowid_input_index; // Index in the input batch containing the rowids for this table
        const Table* source_table;
        size_t source_col_index; // Index in the source table for the column we want to fetch
    };

public:
    MaterializationOperator(ExecutionContext& context, Schema output_schema, std::unique_ptr<AbstractOperator> child);
    ~MaterializationOperator() override = default;

    Result<RowBatch> next() override;

private:
    std::vector<ColumnMapping> col_map_;
    bool produced_;
};

} // namespace velodb
