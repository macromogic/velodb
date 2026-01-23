#include "operator/materialization_operator.hpp"

#include "catalog/catalog.hpp"
#include "catalog/row_batch.hpp"
#include "catalog/schema.hpp"
#include "catalog/table.hpp"

namespace velodb {

MaterializationOperator::MaterializationOperator(ExecutionContext& context,
                                                 Schema output_schema,
                                                 std::unique_ptr<AbstractOperator> child)
    : UnaryOperator(context, std::move(output_schema), std::move(child))
    , produced_(false)
{
    const auto& in_schema = child_->getOutputSchema();
    auto [left_table_name, _] = splitName(in_schema.getColumnInfo(0).getName());
    auto [right_table_name, __] = splitName(in_schema.getColumnInfo(1).getName());
    left_table_ = &context_.getCatalog().getTable(left_table_name).value().get();
    right_table_ = &context_.getCatalog().getTable(right_table_name).value().get();
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

std::pair<std::string, std::string> MaterializationOperator::splitName(const std::string& name)
{
    auto pos = name.find('.');
    if (pos == std::string::npos) {
        return { "", name };
    }
    return { name.substr(0, pos), name.substr(pos + 1) };
}

Result<RowBatch> MaterializationOperator::next()
{
    PROFILE_SCOPE("MaterializationOperator::next");
    // Consume child once (single batch join result assumed currently)
    if (produced_) {
        return Result<RowBatch>::success(RowBatch());
    }
    auto join_batch = collectBatches(*child_);
    if (join_batch.getRowCount() == 0) {
        produced_ = true;
        return Result<RowBatch>::success(RowBatch());
    }
    VELODB_ASSERT_MSG(join_batch.getColumnCount() == 2,
                      "Join batch must have exactly two columns for left and right rowids");
    join_batch.to(DataLocation::HOST);
    size_t num_columns = col_map_.size();
    std::vector<Column> output_columns;
    output_columns.reserve(num_columns);
    for (size_t i = 0; i < num_columns; ++i) {
        const auto& mapping = col_map_[i];
        const Table* source_table = mapping.is_left ? left_table_ : right_table_;
        const Column& source_col = source_table->getColumn(mapping.source_index);
        const Column& rowid_col = join_batch.getColumn(mapping.is_left ? 0 : 1);
        output_columns.push_back(source_col.gather(rowid_col));
    }
    RowBatch materialized_batch = buildBatchFromColumns(std::move(output_columns));
    produced_ = true;
    return Result<RowBatch>::success(std::move(materialized_batch));
}

} // namespace velodb
