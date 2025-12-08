#include "operator/materialization_operator.hpp"

#include "catalog/catalog.hpp"
#include "catalog/execution_context.hpp"
#include "catalog/row_batch.hpp"
#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include "common/exception.hpp"

#include <unordered_map>

namespace velodb {

static std::string strip_rowid_suffix(const std::string& name)
{
    constexpr std::string_view suffix = "$_rowid";
    if (name.size() >= suffix.size() && name.substr(name.size() - suffix.size()) == suffix) {
        auto base = name.substr(0, name.size() - suffix.size());
        if (!base.empty() && base.back() == '.') {
            base.pop_back();
        }
        return base;
    }
    return name;
}

Result<RowBatch> MaterializationOperator::next()
{
    auto* child = getChild();
    if (!child) {
        return Result<RowBatch>::failure("MaterializationOperator requires a child operator");
    }
    // Consume child once (single batch join result assumed currently)
    if (produced_) {
        return Result<RowBatch>::success(RowBatch());
    }
    auto child_result = child->next();
    if (!child_result) {
        return child_result;
    }
    auto join_batch = std::move(child_result.value());
    if (join_batch.getRowCount() == 0) {
        produced_ = true;
        return Result<RowBatch>::success(std::move(join_batch));
    }

    // Expect two columns: left_rowid, right_rowid
    const auto& in_schema = child->getOutputSchema();
    // Derive source table names from column names
    std::string left_table_name = strip_rowid_suffix(in_schema.getColumnInfo(0).getName());
    std::string right_table_name = strip_rowid_suffix(in_schema.getColumnInfo(1).getName());

    auto& catalog = context_.getCatalog();
    auto left_table_opt = catalog.getTable(left_table_name);
    auto right_table_opt = catalog.getTable(right_table_name);
    if (!left_table_opt || !right_table_opt) {
        return Result<RowBatch>::failure("Source table(s) not found for materialization");
    }
    auto& left_table = left_table_opt->get();
    auto& right_table = right_table_opt->get();

    size_t pair_count = join_batch.getRowCount();

    // Prepare vectors for all columns from both tables (excluding internal $_rowid/$_mask if present)
    std::vector<std::vector<Value>> all_columns_values;
    std::vector<std::unique_ptr<DataType>> all_column_types;
    std::vector<std::string> all_column_names; // unused but kept for potential debug

    auto collect_table_meta = [&](const Table& table) {
        for (size_t ci = 0; ci < table.getColumnCount(); ++ci) {
            const auto& col_name = table.getColumnName(ci);
            if (col_name == "$_rowid" || col_name == "$_mask")
                continue; // skip internal
            all_column_names.push_back(col_name);
            all_column_types.push_back(table.getColumnType(ci).cloneUnique());
            all_columns_values.emplace_back();
            all_columns_values.back().reserve(pair_count);
        }
    };
    collect_table_meta(left_table);
    collect_table_meta(right_table);

    // Map to base index for each table's columns in combined vectors
    size_t left_base = 0;
    size_t right_base = 0;
    {
        size_t left_cols = 0;
        for (size_t ci = 0; ci < left_table.getColumnCount(); ++ci) {
            auto cn = left_table.getColumnName(ci);
            if (cn == "$_rowid" || cn == "$_mask")
                continue;
            ++left_cols;
        }
        left_base = 0;
        right_base = left_cols;
    }

    // Fill values by iterating rowid pairs
    join_batch.to(DataLocation::HOST); // Ensure accessible
    for (size_t i = 0; i < pair_count; ++i) {
        auto lrowid_val = join_batch.getValue(i, 0);
        auto rrowid_val = join_batch.getValue(i, 1);
        uint64_t lrowid = static_cast<uint64_t>(lrowid_val.getBigInt());
        uint64_t rrowid = static_cast<uint64_t>(rrowid_val.getBigInt());

        // Left table columns
        size_t dest_index = left_base;
        for (size_t ci = 0; ci < left_table.getColumnCount(); ++ci) {
            auto cn = left_table.getColumnName(ci);
            if (cn == "$_rowid" || cn == "$_mask")
                continue;
            all_columns_values[dest_index++].push_back(left_table.getValue(lrowid, ci));
        }
        // Right table columns
        dest_index = right_base;
        for (size_t ci = 0; ci < right_table.getColumnCount(); ++ci) {
            auto cn = right_table.getColumnName(ci);
            if (cn == "$_rowid" || cn == "$_mask")
                continue;
            all_columns_values[dest_index++].push_back(right_table.getValue(rrowid, ci));
        }
    }

    // Build batch using output schema ordering (already disambiguated)
    RowBatch materialized;
    // We assume planner generated schema ordering matches left columns then right columns; align vectors accordingly.
    for (size_t idx = 0; idx < all_columns_values.size(); ++idx) {
        materialized.addColumn(Column::buildFrom(std::move(all_column_types[idx]), std::move(all_columns_values[idx])));
    }

    produced_ = true;
    return Result<RowBatch>::success(std::move(materialized));
}

} // namespace velodb
