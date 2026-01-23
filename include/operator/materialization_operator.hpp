#pragma once

#include "operator/abstract_operator.hpp"

namespace velodb {

class MaterializationOperator : public UnaryOperator {
    struct ColumnMapping {
        bool is_left;
        size_t source_index;
    };

public:
    MaterializationOperator(ExecutionContext& context, Schema output_schema, std::unique_ptr<AbstractOperator> child);
    ~MaterializationOperator() override = default;

    Result<RowBatch> next() override;

private:
    static std::pair<std::string, std::string> splitName(const std::string& name);

    const Table* left_table_;
    const Table* right_table_;
    std::vector<ColumnMapping> col_map_;
    bool produced_;
};

} // namespace velodb
