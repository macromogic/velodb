#include "catalog/tuple.hpp"
#include "catalog/table.hpp"
#include "common/exception.hpp"
#include <sstream>

namespace velodb {

ValueTuple::ValueTuple(const Schema& schema)
    : schema_(std::cref(schema))
{
    values_.resize(schema.getColumnCount());
    for (size_t i = 0; i < schema.getColumnCount(); ++i) {
        values_[i] = Value::createNull(schema.getColumnInfo(i).getType().getTypeId());
    }
}

ValueTuple::ValueTuple(const Schema& schema, std::vector<Value> values)
    : schema_(std::cref(schema))
    , values_(std::move(values))
{
    if (values_.size() != schema.getColumnCount()) {
        VELODB_THROW(CatalogError, "Value count mismatch with schema");
    }
}

const Value& ValueTuple::getValue(size_t column_index) const
{
    if (column_index >= values_.size()) {
        VELODB_THROW(CatalogError, "Column index out of range");
    }
    return values_[column_index];
}

const Value& ValueTuple::getValue(const std::string& column_name) const
{
    size_t const index = schema_.get().getColumnIndex(column_name);
    return values_[index];
}

std::string ValueTuple::toString() const
{
    std::stringstream ss;
    ss << "(";
    for (size_t i = 0; i < values_.size(); ++i) {
        if (i > 0)
            ss << ", ";
        ss << values_[i].toString();
    }
    ss << ")";
    return ss.str();
}

ViewTuple::ViewTuple(const TableBase& table, uint64_t row_id)
    : table_(table)
    , row_id_(row_id)
{
}

bool ViewTuple::operator==(const ViewTuple& other) const
{
    return &table_ == &other.table_ && row_id_ == other.row_id_;
}

const Value& ViewTuple::getValue(size_t column_index) const
{
    if (column_index >= table_.getSchema().getColumnCount()) {
        VELODB_THROW(CatalogError, "Column index out of range");
    }
    return table_.getValue(row_id_, column_index);
}

const Value& ViewTuple::getValue(const std::string& column_name) const
{
    size_t const index = table_.getSchema().getColumnIndex(column_name);
    return getValue(index);
}

size_t ViewTuple::getColumnCount() const
{
    return table_.getSchema().getColumnCount();
}

std::string ViewTuple::toString() const
{
    std::stringstream ss;
    ss << "(";
    for (size_t i = 0; i < table_.getSchema().getColumnCount(); ++i) {
        if (i > 0)
            ss << ", ";
        ss << getValue(i).toString();
    }
    ss << ")";
    return ss.str();
}

} // namespace velodb
