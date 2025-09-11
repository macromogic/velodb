#include "catalog/schema.hpp"

#include "catalog/column.hpp"
#include "common/exception.hpp"
#include "common/fmt.hpp"

#include <fmt/ranges.h>

#include <stdexcept>
#include <utility>

namespace velodb {

Schema::Schema(std::vector<ColumnInfo> columns)
    : columns_(std::move(columns))
{
    for (size_t i = 0; i < columns_.size(); ++i) {
        column_name_to_index_[columns_[i].getName()] = i;
    }
}

void Schema::addColumnInfo(ColumnInfo column)
{
    column_name_to_index_[column.getName()] = columns_.size();
    columns_.push_back(std::move(column));
}

const ColumnInfo& Schema::getColumnInfo(size_t index) const
{
    if (index >= columns_.size()) {
        VELODB_THROW(SchemaError, "Column index out of range");
    }
    return columns_[index];
}

const ColumnInfo& Schema::getColumnInfo(const std::string& name) const
{
    auto it = column_name_to_index_.find(name);
    if (it == column_name_to_index_.end()) {
        VELODB_THROW(SchemaError, "Column not found: " + name);
    }
    return columns_[it->second];
}

size_t Schema::getColumnIndex(const std::string& name) const
{
    auto it = column_name_to_index_.find(name);
    if (it == column_name_to_index_.end()) {
        VELODB_THROW(SchemaError, "Column not found: " + name);
    }
    return it->second;
}

bool Schema::hasColumn(const std::string& name) const
{
    return column_name_to_index_.find(name) != column_name_to_index_.end();
}

Schema Schema::cloneImpl() const
{
    std::vector<ColumnInfo> cloned_columns;
    cloned_columns.reserve(columns_.size());
    for (const auto& column : columns_) {
        cloned_columns.push_back(column.clone());
    }
    return Schema(std::move(cloned_columns));
}

std::string Schema::toString() const
{
    return fmt::format("({})", fmt::join(columns_, ", "));
}

} // namespace velodb
