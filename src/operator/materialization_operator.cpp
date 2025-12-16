#include "operator/materialization_operator.hpp"

#include "catalog/catalog.hpp"
#include "catalog/row_batch.hpp"
#include "catalog/schema.hpp"
#include "catalog/table.hpp"

namespace velodb {

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

    auto left_rows = left_table_->getRowCount();
    auto right_rows = right_table_->getRowCount();
    RowBatch left_buf = left_table_->slice(0, left_rows);
    RowBatch right_buf = right_table_->slice(0, right_rows);
    auto& left_rowids = join_batch.getColumn(0);
    auto& right_rowids = join_batch.getColumn(1);

    std::vector<std::reference_wrapper<Column>> src_columns;
    std::vector<std::reference_wrapper<const Column>> rowid_columns;
    for (const auto& [is_left, source_index] : col_map_) {
        if (is_left) {
            src_columns.push_back(std::ref(left_buf.getColumn(source_index)));
            rowid_columns.push_back(std::cref(left_rowids));
        } else {
            src_columns.push_back(std::ref(right_buf.getColumn(source_index)));
            rowid_columns.push_back(std::cref(right_rowids));
        }
    }
    auto materialized_batch = RowBatch::materializeColumns(src_columns, rowid_columns);
    // materialized_batch.to(DataLocation::HOST);
    produced_ = true;
    return Result<RowBatch>::success(std::move(materialized_batch));
}

} // namespace velodb
