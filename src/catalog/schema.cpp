#include "catalog/schema.hpp"
#include "common/exception.hpp"
#include <sstream>
#include <stdexcept>
#include <utility>

namespace velodb {

Schema::Schema(std::vector<ColumnInfo> columns)
    : columns_(std::move(columns))
{
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

std::unique_ptr<Schema> Schema::cloneUniqueImpl() const
{
    std::vector<ColumnInfo> cloned_columns;
    cloned_columns.reserve(columns_.size());
    for (const auto& column : columns_) {
        cloned_columns.push_back(column.clone());
    }
    return std::make_unique<Schema>(std::move(cloned_columns));
}

std::string Schema::toString() const
{
    std::stringstream ss;
    ss << "(";
    for (size_t i = 0; i < columns_.size(); ++i) {
        if (i > 0)
            ss << ", ";
        ss << columns_[i].toString();
    }
    ss << ")";
    return ss.str();
}

} // namespace velodb
