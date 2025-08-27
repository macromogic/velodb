#include "catalog/column/value_column.hpp"

#include "catalog/column/column_info.hpp"
#include "catalog/column/view_column.hpp"
#include "common/exception.hpp"
#include "common/fmt.hpp"

#include <fmt/format.h>

namespace velodb {

ValueColumn::ValueColumn(std::string name,
                         std::unique_ptr<DataType> type,
                         bool is_nullable,
                         bool is_unique,
                         bool is_primary_key)
    : ColumnBase(std::move(name), is_nullable, is_unique, is_primary_key)
    , type_(type->cloneUnique())
    , values_(std::move(type))
{
}

ValueColumn::ValueColumn(const ColumnInfo& info)
    : ColumnBase(info.getName(), info.isNullable(), info.isUnique(), info.isPrimaryKey())
    , type_(info.getType().cloneUnique())
    , values_(info.getType().cloneUnique())
{
}

void ValueColumn::resize(size_t new_size)
{
    values_.resize(new_size, Value::createNull(type_->getTypeId()));
}

void ValueColumn::reserve(size_t new_capacity)
{
    values_.reserve(new_capacity);
}

size_t ValueColumn::size() const
{
    return values_.size();
}

Value ValueColumn::get(size_t row) const
{
    if (row >= values_.size()) {
        VELODB_THROW(CatalogError, "Row index out of range");
    }
    return values_.get(row);
}

Value ValueColumn::operator[](size_t row)
{
    if (row >= values_.size()) {
        VELODB_THROW(CatalogError, "Row index out of range");
    }
    return values_.get(row);
}

void ValueColumn::append(const Value& value)
{
    if (value.getTypeId() != type_->getTypeId()) {
        VELODB_THROW(CatalogError, "Value type does not match column type");
    }
    values_.append(value);
}

void ValueColumn::fill(const Value& value, size_t count)
{
    values_.clear();
    values_.resize(count, value);
}

ViewColumn ValueColumn::view() const
{
    return { *type_, getName(), values_ };
}

ViewColumn ValueColumn::viewAs(std::string alias) const
{
    return { *type_, std::move(alias), values_ };
}

ViewColumn ValueColumn::slice(size_t start_row, size_t end_row) const
{
    if (start_row > end_row || end_row > values_.size()) {
        VELODB_THROW(CatalogError, "Invalid slice range");
    }
    return { *type_, getName(), values_, start_row, end_row };
}

ViewColumn ValueColumn::indices(const std::vector<size_t>& indices) const
{
    // Validate all indices are within bounds
    for (size_t idx : indices) {
        if (idx >= values_.size()) {
            VELODB_THROW(CatalogError, "Index out of range in column indices view");
        }
    }
    return { *type_, getName(), values_, indices };
}

ViewColumn ValueColumn::filterValues(std::function<bool(const Value&)> predicate) const
{
    std::vector<size_t> matching_indices;
    for (size_t i = 0; i < values_.size(); ++i) {
        if (predicate(values_[i])) {
            matching_indices.push_back(i);
        }
    }
    return { *type_, getName(), values_, std::move(matching_indices) };
}

std::string ValueColumn::toString() const
{
    std::string result = fmt::format("ValueColumn(name={}, type={}, size={}", getName(), *type_, size());
    if (!isNullable()) {
        result += ", not null";
    }
    if (isUnique()) {
        result += ", unique";
    }
    if (isPrimaryKey()) {
        result += ", primary key";
    }
    result += ")";
    return result;
}

} // namespace velodb
