#include "catalog/table.hpp"

#include "catalog/column.hpp"
#include "catalog/row_batch.hpp"
#include "common/exception.hpp"
#include "common/profiler.hpp"

#include <fmt/core.h>

#include <utility>

namespace velodb {

Table::Table(std::string name, Schema schema)
    : name_(std::move(name))
    , schema_(std::move(schema))
    , row_count_(0)
{
}

void Table::initializeColumns()
{
    PROFILE_SCOPE("initializeColumns");
    const size_t column_count = schema_.getColumnCount();
    {
        {
            PROFILE_SCOPE("columns reserve");
            columns_.reserve(column_count);
        }

        {
            PROFILE_SCOPE("columns creation loop");
            for (auto& info : schema_) {
                PROFILE_SCOPE("single column emplace_back");
                columns_.emplace_back(info.getType().cloneUnique());
            }
        }
    }
}

size_t Table::getColumnIndex(const std::string& name) const
{
    return schema_.getColumnIndex(name);
}

const std::string& Table::getColumnName(size_t index) const
{
    return schema_.getColumnInfo(index).getName();
}

const DataType& Table::getColumnType(const std::string& name) const
{
    return schema_.getColumnInfo(name).getType();
}

const DataType& Table::getColumnType(size_t index) const
{
    return schema_.getColumnInfo(index).getType();
}

const Column& Table::getColumn(const std::string& name) const
{
    size_t column_index = getColumnIndex(name);
    return getColumn(column_index);
}

const Column& Table::getColumn(size_t column_index) const
{
    if (column_index >= columns_.size()) {
        VELODB_THROW(CatalogError, "Column index out of range");
    }
    return columns_[column_index];
}

// Efficient column-based access for late materialization
const Value Table::getValue(size_t row_id, size_t column_index) const
{
    if (row_id >= row_count_) {
        VELODB_THROW(CatalogError, "Row ID out of range");
    }
    if (column_index >= columns_.size()) {
        VELODB_THROW(CatalogError, "Column index out of range");
    }

    return columns_[column_index].get(row_id);
}

std::string Table::toString() const
{
    return fmt::format("{} {}", name_, schema_);
}

RowBatch Table::slice(size_t begin, size_t end) const
{
    if (begin > end || end > row_count_) {
        VELODB_THROW(CatalogError, "Invalid slice range");
    }

    RowBatch batch;
    for (const auto& column : columns_) {
        batch.addColumn(column.slice(begin, end));
    }
    return batch;
}

} // namespace velodb
