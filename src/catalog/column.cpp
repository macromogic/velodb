#include "catalog/column.hpp"
#include "common/exception.hpp"
#include <sstream>

namespace velodb {

ColumnInfo::ColumnInfo(std::string name,
    std::unique_ptr<DataType> type,
    bool is_nullable,
    bool is_unique,
    bool is_primary_key)
    : name_(std::move(name))
    , type_(std::move(type))
    , is_nullable_(is_nullable)
    , is_unique_(is_unique)
    , is_primary_key_(is_primary_key)
{
}

ColumnInfo ColumnInfo::cloneImpl() const
{
    return { name_, type_->cloneUnique(), is_nullable_, is_unique_, is_primary_key_ };
}

std::string ColumnInfo::toString() const
{
    std::stringstream ss;
    ss << name_ << " " << type_->toString();
    if (!is_nullable_) {
        ss << " NOT NULL";
    }
    if (is_unique_) {
        ss << " UNIQUE";
    }
    if (is_primary_key_) {
        ss << " PRIMARY KEY";
    }
    return ss.str();
}

Column::Column(std::string name,
    bool is_nullable,
    bool is_unique,
    bool is_primary_key)
    : name_(std::move(name))
    , is_nullable_(is_nullable)
    , is_unique_(is_unique)
    , is_primary_key_(is_primary_key)
{
}

ValueColumn::ValueColumn(std::string name,
    std::unique_ptr<DataType> type,
    bool is_nullable,
    bool is_unique,
    bool is_primary_key)
    : Column(std::move(name), is_nullable, is_unique, is_primary_key)
    , type_(std::move(type))
{
}

ValueColumn::ValueColumn(const ColumnInfo& info)
    : Column(info.getName(), info.isNullable(), info.isUnique(), info.isPrimaryKey())
    , type_(info.getType().cloneUnique())
{
}

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

Value& ValueColumn::operator[](size_t row)
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

ViewColumn::ViewColumn(DataType& type, std::string name, const ValueVector& values)
    : Column(std::move(name), true, false, false)
    , type_(type)
    , values_(values)
{
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

ViewColumn ViewColumn::view() const
{
    return { type_, getName(), values_ };
}

ViewColumn ViewColumn::viewAs(std::string alias) const
{
    return { type_, std::move(alias), values_ };
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
