#include "catalog/table.hpp"
#include <sstream>
#include <stdexcept>
#include <utility>

namespace velodb {

// Tuple implementation
Tuple::Tuple(const Schema& schema)
    : schema_(schema)
{
    values_.resize(schema.getColumnCount());
    for (size_t i = 0; i < schema.getColumnCount(); ++i) {
        values_[i] = Value::createNull(schema.getColumn(i).getType().getTypeId());
    }
}

Tuple::Tuple(const Schema& schema, std::vector<Value> values)
    : schema_(schema)
    , values_(std::move(values))
{
    if (values_.size() != schema.getColumnCount()) {
        throw std::invalid_argument("Value count mismatch with schema");
    }
}

const Value& Tuple::getValue(size_t column_index) const
{
    if (column_index >= values_.size()) {
        throw std::out_of_range("Column index out of range");
    }
    return values_[column_index];
}

const Value& Tuple::getValue(const std::string& column_name) const
{
    size_t const index = schema_.getColumnIndex(column_name);
    return values_[index];
}

void Tuple::setValue(size_t column_index, const Value& value)
{
    if (column_index >= values_.size()) {
        throw std::out_of_range("Column index out of range");
    }
    values_[column_index] = value;
}

void Tuple::setValue(const std::string& column_name, const Value& value)
{
    size_t const index = schema_.getColumnIndex(column_name);
    values_[index] = value;
}

std::string Tuple::toString() const
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

// TableInfo implementation
TableInfo::TableInfo(std::string name, std::unique_ptr<Schema> schema)
    : name_(std::move(name))
    , schema_(std::move(schema))
{
}

// TableBase implementation
TableBase::TableBase(std::unique_ptr<TableInfo> table_info)
    : table_info_(std::move(table_info))
{
}

// Table implementation
Table::Table(std::unique_ptr<TableInfo> table_info)
    : TableBase(std::move(table_info))
{
}

std::unique_ptr<TableIterator> Table::getIterator()
{
    return std::make_unique<TableIterator>(*this);
}

void Table::insertTuple(const Tuple& tuple)
{
    tuples_.push_back(tuple);
    deleted_rows_.push_back(false);
}

void Table::insertTuple(Tuple&& tuple)
{
    tuples_.push_back(std::move(tuple));
    deleted_rows_.push_back(false);
}

const Tuple& Table::getTuple(RowId row_id) const
{
    if (row_id >= tuples_.size()) {
        throw std::out_of_range("Row ID out of range");
    }
    if (deleted_rows_[row_id]) {
        throw std::runtime_error("Row has been deleted");
    }
    return tuples_[row_id];
}

bool Table::deleteTuple(RowId row_id)
{
    if (row_id >= tuples_.size()) {
        return false;
    }
    deleted_rows_[row_id] = true;
    return true;
}

Value Table::getValue(RowId row_id, size_t column_index) const
{
    const Tuple& tuple = getTuple(row_id);
    return tuple.getValue(column_index);
}

std::vector<Value> Table::getValues(RowId row_id, const std::vector<size_t>& column_indices) const
{
    const Tuple& tuple = getTuple(row_id);
    std::vector<Value> values;
    values.reserve(column_indices.size());

    for (size_t const col_idx : column_indices) {
        values.push_back(tuple.getValue(col_idx));
    }
    return values;
}

// View implementation
View::View(std::unique_ptr<TableInfo> table_info, std::vector<Tuple> materialized_tuples)
    : TableBase(std::move(table_info))
    , tuples_(std::move(materialized_tuples))
{
}

std::unique_ptr<TableIterator> View::getIterator()
{
    return std::make_unique<TableIterator>(*this);
}

const Tuple& View::getTuple(size_t index) const
{
    if (index >= tuples_.size()) {
        throw std::out_of_range("Index out of range");
    }
    return tuples_[index];
}

// TableIterator implementation
TableIterator::TableIterator(const Table& table)
    : table_(table)
    , current_index_(0)
    , current_row_id_(0)
    , is_view_(false)
{
}

TableIterator::TableIterator(const View& view)
    : table_(view)
    , current_index_(0)
    , current_row_id_(0)
    , is_view_(true)
{
}

bool TableIterator::hasNext() const
{
    if (is_view_) {
        const View& view = dynamic_cast<const View&>(table_);
        return current_index_ < view.getRowCount();
    }
    const Table& table = static_cast<const Table&>(table_);
    // Skip deleted rows
    size_t index = current_index_;
    while (index < table.getRowCount() && table.deleted_rows_[index]) {
        index++;
    }
    return index < table.getRowCount();
}

const Tuple& TableIterator::next()
{
    if (!hasNext()) {
        throw std::runtime_error("No more tuples");
    }

    if (is_view_) {
        const View& view = dynamic_cast<const View&>(table_);
        current_row_id_ = current_index_;
        return view.getTuple(current_index_++);
    }
    const Table& table = static_cast<const Table&>(table_);
    // Skip deleted rows
    while (current_index_ < table.getRowCount() && table.deleted_rows_[current_index_]) {
        current_index_++;
    }
    current_row_id_ = current_index_;
    return table.getTuple(current_index_++);
}

void TableIterator::reset()
{
    current_index_ = 0;
    current_row_id_ = 0;
}

} // namespace velodb
