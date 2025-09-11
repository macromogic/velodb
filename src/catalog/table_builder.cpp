#include "catalog/table_builder.hpp"

#include "catalog/column.hpp"
#include "common/exception.hpp"
#include "data/data_type.hpp"

#include <fmt/core.h>

namespace velodb {

TableBuilder::TableBuilder(std::string name, Schema schema)
    : name_(std::move(name))
    , schema_(std::move(schema))
    , num_columns_(schema_.getColumnCount())
{
    pending_columns_.resize(num_columns_);
}

TableBuilder& TableBuilder::insertRow(const std::vector<Value>& values)
{
    validateRowSize(values);
    for (size_t i = 0; i < num_columns_; ++i) {
        pending_columns_[i].push_back(values[i]);
    }
    num_rows_++;
    return *this;
}

TableBuilder& TableBuilder::insertRow(std::vector<Value>&& values)
{
    validateRowSize(values);
    for (size_t i = 0; i < num_columns_; ++i) {
        pending_columns_[i].push_back(std::move(values[i]));
    }
    num_rows_++;
    return *this;
}

TableBuilder& TableBuilder::insertRows(const std::vector<std::vector<Value>>& rows)
{
    for (const auto& row : rows) {
        validateRowSize(row);
        for (size_t i = 0; i < num_columns_; ++i) {
            pending_columns_[i].push_back(row[i]);
        }
        num_rows_++;
    }
    return *this;
}

TableBuilder& TableBuilder::insertRows(std::vector<std::vector<Value>>&& rows)
{
    for (auto& row : rows) {
        validateRowSize(row);
        for (size_t i = 0; i < num_columns_; ++i) {
            pending_columns_[i].push_back(std::move(row[i]));
        }
        num_rows_++;
    }
    return *this;
}

TableBuilder& TableBuilder::reserveRows(size_t row_count)
{
    for (auto& column : pending_columns_) {
        column.reserve(row_count);
    }
    return *this;
}

Table TableBuilder::build() &&
{
    Table table(std::move(name_), std::move(schema_));

    std::vector<Column> columns;
    columns.reserve(num_columns_);
    for (size_t i = 0; i < num_columns_; ++i) {
        auto column = Column::buildFrom(table.getColumnType(i).cloneUnique(), std::move(pending_columns_[i]));
        columns.push_back(std::move(column));
    }
    table.columns_ = std::move(columns);
    table.row_count_ = num_rows_;

    return table;
}

void TableBuilder::validateRowSize(const std::vector<Value>& row) const
{
    if (row.size() != num_columns_) {
        VELODB_THROW(CatalogError, fmt::format("Row has {} values but schema expects {}", row.size(), num_columns_));
    }
}

} // namespace velodb
