#pragma once

#include "operator/abstract_operator.hpp"

namespace velodb {

class MaterializationOperator : public UnaryOperator {
    struct ColumnMapping {
        bool is_left;
        size_t source_index;
    };

public:
    MaterializationOperator(ExecutionContext& context, Schema output_schema, std::unique_ptr<AbstractOperator> child)
        : UnaryOperator(context, std::move(output_schema), std::move(child))
        , produced_(false)
    {
        const auto& in_schema = getChild()->getOutputSchema();
        auto [left_table_name, _] = splitName(in_schema.getColumnInfo(0).getName());
        auto [right_table_name, __] = splitName(in_schema.getColumnInfo(1).getName());
        left_table_ = &context.getCatalog().getTable(left_table_name).value().get();
        right_table_ = &context.getCatalog().getTable(right_table_name).value().get();
        VELODB_ASSERT_MSG(left_table_ && right_table_, "Source tables must exist");

        const auto& out_schema = getOutputSchema();
        // Build column maps
        for (size_t i = 0; i < out_schema.getColumnCount(); ++i) {
            const auto& col_info = out_schema.getColumnInfo(i);
            auto [table_name, col_name] = splitName(col_info.getName());
            if (table_name.empty()) {
                for (size_t j = 0; j < left_table_->getColumnCount(); ++j) {
                    if (left_table_->getColumnName(j) == col_name) {
                        col_map_.push_back({ true, j });
                        goto next_column;
                    }
                }
                for (size_t j = 0; j < right_table_->getColumnCount(); ++j) {
                    if (right_table_->getColumnName(j) == col_name) {
                        col_map_.push_back({ false, j });
                        goto next_column;
                    }
                }
                VELODB_THROW(DatabaseError, "Output schema column does not match either source table");
            next_column:;
            } else if (table_name == left_table_->getName()) {
                size_t col_index = left_table_->getColumnIndex(col_name);
                col_map_.push_back({ true, col_index });
            } else if (table_name == right_table_->getName()) {
                size_t col_index = right_table_->getColumnIndex(col_name);
                col_map_.push_back({ false, col_index });
            } else {
                VELODB_THROW(DatabaseError, "Output schema column does not match either source table");
            }
        }
    }
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
