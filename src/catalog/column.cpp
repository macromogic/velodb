#include "catalog/column.hpp"
#include "common/exception.hpp"
#include <sstream>

namespace velodb {

void ValueColumn::resize(size_t new_size)
{
    values_.resize(new_size);
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
    return values_[row];
}

void ValueColumn::append(const Value& value)
{
    if (value.getTypeId() != type_->getTypeId()) {
        VELODB_THROW(CatalogError, "Value type does not match column type");
    }
    values_.push_back(value);
}

ViewColumn ValueColumn::view() const
{
    return { *type_, getName(), values_ };
}

ViewColumn ValueColumn::viewAs(std::string alias) const
{
    return { *type_, std::move(alias), values_ };
}

std::string ValueColumn::toString() const
{
    std::stringstream ss;
    ss << "ValueColumn(name=" << getName() << ", type=" << type_->toString() << ", size=" << size();
    if (!isNullable()) {
        ss << ", not null";
    }
    if (isUnique()) {
        ss << ", unique";
    }
    if (isPrimaryKey()) {
        ss << ", primary key";
    }
    ss << ")";
    return ss.str();
}

size_t ViewColumn::size() const
{
    return values_.size();
}

Value ViewColumn::get(size_t row) const
{
    if (row >= values_.size()) {
        VELODB_THROW(CatalogError, "Row index out of range");
    }
    return values_[row];
}

std::string ViewColumn::toString() const
{
    std::stringstream ss;
    ss << "ViewColumn(name=" << getName() << ", type=" << type_.toString() << ", size=" << size();
    if (!isNullable()) {
        ss << ", not null";
    }
    if (isUnique()) {
        ss << ", unique";
    }
    if (isPrimaryKey()) {
        ss << ", primary key";
    }
    ss << ")";
    return ss.str();
}

} // namespace velodb