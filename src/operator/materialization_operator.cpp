#include "operator/materialization_operator.hpp"

#include "catalog/catalog.hpp"
#include "catalog/row_batch.hpp"
#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include "common/string_utils.hpp"

namespace velodb {

MaterializationOperator::MaterializationOperator(ExecutionContext& context,
                                                 Schema output_schema,
                                                 std::unique_ptr<AbstractOperator> child)
    : UnaryOperator(context, std::move(output_schema), std::move(child))
{
    // Identify available tables and their RowID column index from the input child operator
    // The child operator (Projection of RowIDs) must output columns named "TableName.$_rowid"
    std::unordered_map<std::string, size_t> table_rowid_indices;
    std::unordered_map<std::string, const Table*> tables;

    const auto& in_schema = child_->getOutputSchema();
    for (size_t i = 0; i < in_schema.getColumnCount(); ++i) {
        auto full_name = in_schema.getColumnInfo(i).getName();
        auto [table_name_view, col_name_view] = splitName(full_name);

        // We expect inputs to be rowids.
        if (col_name_view == "$_rowid") {
            std::string table_name(table_name_view);
            table_rowid_indices[table_name] = i;
            if (tables.find(table_name) == tables.end()) {
                auto table_opt = context_.getCatalog().getTable(table_name);
                VELODB_ASSERT_MSG(table_opt.has_value(), "Source table must exist in catalog");
                tables[table_name] = &table_opt.value().get();
            }
        }
    }

    const auto& out_schema = getOutputSchema();
    // Build column maps
    for (size_t i = 0; i < out_schema.getColumnCount(); ++i) {
        const auto& col_info = out_schema.getColumnInfo(i);
        auto full_name = col_info.getName();
        auto [table_name_view, col_name_view] = splitName(full_name);
        std::string table_name(table_name_view);
        std::string col_name(col_name_view);

        const Table* source_table = nullptr;
        size_t rowid_idx = 0;

        if (table_name.empty()) {
            for (const auto& [t_name, t_ptr] : tables) {
                bool found = false;
                for (size_t j = 0; j < t_ptr->getColumnCount(); ++j) {
                    if (t_ptr->getColumnName(j) == col_name) {
                        found = true;
                        break;
                    }
                }

                if (found) {
                    if (source_table != nullptr) {
                        VELODB_THROW(DatabaseError, "Ambiguous column name in materialization: " + col_name);
                    }
                    source_table = t_ptr;
                    rowid_idx = table_rowid_indices[t_name];
                }
            }
            if (source_table == nullptr) {
                VELODB_THROW(DatabaseError, "Output schema column does not match any source table: " + col_name);
            }
        } else {
            auto it = tables.find(table_name);
            if (it == tables.end()) {
                VELODB_THROW(DatabaseError, "Table not available in join results: " + table_name);
            }
            source_table = it->second;
            rowid_idx = table_rowid_indices[table_name];
        }

        // Find column index in source table
        size_t source_col_idx = source_table->getColumnIndex(col_name);
        col_map_.push_back({ rowid_idx, source_table, source_col_idx });
    }
}

Result<RowBatch> MaterializationOperator::next()
{
    // Streaming mode: get one batch from upstream, materialize it, return
    auto result = child_->next();
    if (!result) {
        return result;
    }

    auto join_batch = std::move(result.value());
    if (join_batch.getRowCount() == 0) {
        return Result<RowBatch>::success(RowBatch()); // End of stream
    }

    PROFILE_SCOPE("MaterializationOperator::next");
    // Use HOST_PAGEABLE to avoid exhausting pinned memory pool
    // StagedTransfer will handle the D2H transfer in chunks
    join_batch.to(DataLocation::HOST_PAGEABLE);

    size_t num_columns = col_map_.size();
    std::vector<Column> output_columns;
    output_columns.reserve(num_columns);

    for (const auto& mapping : col_map_) {
        const Column& rowid_col = join_batch.getColumn(mapping.rowid_input_index);
        const Column& source_col = mapping.source_table->getColumn(mapping.source_col_index);
        output_columns.push_back(source_col.gather(rowid_col));
    }

    RowBatch materialized_batch = buildBatchFromColumns(std::move(output_columns));
    setNumRowsForBatch(materialized_batch, join_batch.getRowCount());
    return Result<RowBatch>::success(std::move(materialized_batch));
}

} // namespace velodb
