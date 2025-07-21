#include "catalog/table.hpp"
#include <sstream>
#include <stdexcept>
#include <utility>

namespace velodb {

// Tuple implementation
Tuple::Tuple(const Schema& schema)
    : schema_(std::cref(schema))
{
    values_.resize(schema.getColumnCount());
    for (size_t i = 0; i < schema.getColumnCount(); ++i) {
        values_[i] = Value::createNull(schema.getColumn(i).getType().getTypeId());
    }
}

Tuple::Tuple(const Schema& schema, std::vector<Value> values)
    : schema_(std::cref(schema))
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
    size_t const index = schema_.get().getColumnIndex(column_name);
    return values_[index];
}

void Tuple::setValue(size_t column_index, const Value& value)
{
    if (column_index >= values_.size()) {
        throw std::out_of_range("Column index out of range");
    }
    values_[column_index] = value;
}

void Tuple::setValue(size_t column_index, Value&& value)
{
    if (column_index >= values_.size()) {
        throw std::out_of_range("Column index out of range");
    }
    values_[column_index] = std::move(value);
}

void Tuple::setValue(const std::string& column_name, const Value& value)
{
    size_t const index = schema_.get().getColumnIndex(column_name);
    setValue(index, value);
}

void Tuple::setValue(const std::string& column_name, Value&& value)
{
    size_t const index = schema_.get().getColumnIndex(column_name);
    setValue(index, std::move(value));
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

// Table implementation with column-based storage
Table::Table(std::unique_ptr<TableInfo> table_info)
    : TableBase(std::move(table_info))
    , row_count_(0)
{
    initializeColumns();
}

void Table::initializeColumns()
{
    const size_t column_count = table_info_->getColumnCount();
    columns_.resize(column_count);
    // Each column vector starts empty and will grow as rows are inserted
}

void Table::ensureColumnCapacity(size_t new_row_count)
{
    for (auto& column : columns_) {
        if (new_row_count > column.size()) {
            // Resize columns to accommodate new rows
            // Fill with null values for the appropriate type
            const size_t old_size = column.size();
            column.resize(new_row_count);
            
            // Initialize new slots with null values
            for (size_t i = old_size; i < new_row_count; ++i) {
                if (!column.empty()) {
                    // Use the type from existing values in the column
                    column[i] = Value::createNull(column[0].getTypeId());
                }
            }
        }
    }
}

std::unique_ptr<TableIterator> Table::getIterator() const
{
    return std::make_unique<TableIterator>(*this);
}

// Primary column-based insertion methods
void Table::insertRow(const std::vector<Value>& values)
{
    insertRowInternal(values);
}

void Table::insertRow(std::vector<Value>&& values)
{
    if (values.size() != table_info_->getColumnCount()) {
        throw std::invalid_argument("Value count mismatch with schema");
    }
    
    const RowId new_row_id = row_count_;
    ensureColumnCapacity(row_count_ + 1);
    
    // Move each value into its respective column
    for (size_t col_idx = 0; col_idx < values.size(); ++col_idx) {
        columns_[col_idx][new_row_id] = std::move(values[col_idx]);
    }
    
    ++row_count_;
}

void Table::insertBatchRows(const std::vector<std::vector<Value>>& rows)
{
    if (rows.empty()) {
        return;
    }
    
    const size_t old_row_count = row_count_;
    const size_t new_row_count = row_count_ + rows.size();
    ensureColumnCapacity(new_row_count);
    
    // Insert all rows in batch for better performance
    for (size_t row_idx = 0; row_idx < rows.size(); ++row_idx) {
        const std::vector<Value>& values = rows[row_idx];
        if (values.size() != table_info_->getColumnCount()) {
            throw std::invalid_argument("Value count mismatch with schema");
        }
        
        const RowId row_id = old_row_count + row_idx;
        for (size_t col_idx = 0; col_idx < values.size(); ++col_idx) {
            columns_[col_idx][row_id] = values[col_idx];
        }
    }
    
    row_count_ = new_row_count;
}

void Table::insertRowInternal(const std::vector<Value>& values)
{
    if (values.size() != table_info_->getColumnCount()) {
        throw std::invalid_argument("Value count mismatch with schema");
    }
    
    const RowId new_row_id = row_count_;
    ensureColumnCapacity(row_count_ + 1);
    
    // Insert each value into its respective column
    for (size_t col_idx = 0; col_idx < values.size(); ++col_idx) {
        columns_[col_idx][new_row_id] = values[col_idx];
    }
    
    ++row_count_;
}

// Legacy Tuple-based methods for backward compatibility
void Table::insertTuple(const Tuple& tuple)
{
    if (tuple.getColumnCount() != table_info_->getColumnCount()) {
        throw std::invalid_argument("Tuple column count mismatch");
    }
    
    // Convert tuple to vector of values and use column-based insertion
    std::vector<Value> values;
    values.reserve(tuple.getColumnCount());
    for (size_t col_idx = 0; col_idx < tuple.getColumnCount(); ++col_idx) {
        values.push_back(tuple.getValue(col_idx));
    }
    
    insertRowInternal(values);
}

void Table::insertTuple(Tuple&& tuple)
{
    if (tuple.getColumnCount() != table_info_->getColumnCount()) {
        throw std::invalid_argument("Tuple column count mismatch");
    }
    
    // Convert tuple to vector of values and use column-based insertion
    std::vector<Value> values;
    values.reserve(tuple.getColumnCount());
    for (size_t col_idx = 0; col_idx < tuple.getColumnCount(); ++col_idx) {
        values.push_back(std::move(const_cast<Tuple&>(tuple).getValue(col_idx)));
    }
    
    insertRow(std::move(values));
}

Tuple Table::getTuple(RowId row_id) const
{
    if (row_id >= row_count_) {
        throw std::out_of_range("Row ID out of range");
    }
    
    // Reconstruct tuple from column-based storage
    std::vector<Value> values;
    values.reserve(columns_.size());
    
    for (size_t col_idx = 0; col_idx < columns_.size(); ++col_idx) {
        values.push_back(columns_[col_idx][row_id]);
    }
    
    return Tuple(getSchema(), std::move(values));
}

Value Table::getValue(RowId row_id, size_t column_index) const
{
    if (row_id >= row_count_) {
        throw std::out_of_range("Row ID out of range");
    }
    if (column_index >= columns_.size()) {
        throw std::out_of_range("Column index out of range");
    }
    
    return columns_[column_index][row_id];
}

std::vector<Value> Table::getValues(RowId row_id, const std::vector<size_t>& column_indices) const
{
    if (row_id >= row_count_) {
        throw std::out_of_range("Row ID out of range");
    }
    
    std::vector<Value> values;
    values.reserve(column_indices.size());
    
    for (size_t const col_idx : column_indices) {
        if (col_idx >= columns_.size()) {
            throw std::out_of_range("Column index out of range");
        }
        values.push_back(columns_[col_idx][row_id]);
    }
    return values;
}

const ValueVector& Table::getColumn(size_t column_index) const
{
    if (column_index >= columns_.size()) {
        throw std::out_of_range("Column index out of range");
    }
    return columns_[column_index];
}

ValueVector& Table::getColumn(size_t column_index)
{
    if (column_index >= columns_.size()) {
        throw std::out_of_range("Column index out of range");
    }
    return columns_[column_index];
}

void Table::insertBatch(const std::vector<Tuple>& tuples)
{
    if (tuples.empty()) {
        return;
    }
    
    const size_t old_row_count = row_count_;
    const size_t new_row_count = row_count_ + tuples.size();
    ensureColumnCapacity(new_row_count);
    
    // Insert all tuples in batch for better performance
    for (size_t tuple_idx = 0; tuple_idx < tuples.size(); ++tuple_idx) {
        const Tuple& tuple = tuples[tuple_idx];
        if (tuple.getColumnCount() != table_info_->getColumnCount()) {
            throw std::invalid_argument("Tuple column count mismatch");
        }
        
        const RowId row_id = old_row_count + tuple_idx;
        for (size_t col_idx = 0; col_idx < tuple.getColumnCount(); ++col_idx) {
            columns_[col_idx][row_id] = tuple.getValue(col_idx);
        }
    }
    
    row_count_ = new_row_count;
}

std::vector<Value> Table::getColumnValues(size_t column_index, const std::vector<RowId>& row_ids) const
{
    if (column_index >= columns_.size()) {
        throw std::out_of_range("Column index out of range");
    }
    
    std::vector<Value> values;
    values.reserve(row_ids.size());
    
    for (RowId const row_id : row_ids) {
        if (row_id >= row_count_) {
            throw std::out_of_range("Row ID out of range");
        }
        values.push_back(columns_[column_index][row_id]);
    }
    
    return values;
}

std::vector<ValueVector> Table::getColumns(const std::vector<size_t>& column_indices) const
{
    std::vector<ValueVector> result;
    result.reserve(column_indices.size());
    
    for (size_t const col_idx : column_indices) {
        if (col_idx >= columns_.size()) {
            throw std::out_of_range("Column index out of range");
        }
        result.push_back(columns_[col_idx]);
    }
    
    return result;
}

std::vector<RowId> Table::getAllRowIds() const
{
    std::vector<RowId> row_ids;
    row_ids.reserve(row_count_);
    
    for (RowId row_id = 0; row_id < row_count_; ++row_id) {
        row_ids.push_back(row_id);
    }
    
    return row_ids;
}

std::vector<RowId> Table::getValidRowIds() const
{
    std::vector<RowId> row_ids;
    row_ids.reserve(row_count_);
    
    for (RowId row_id = 0; row_id < row_count_; ++row_id) {
        row_ids.push_back(row_id);
    }
    
    return row_ids;
}

// View implementation
View::View(std::unique_ptr<TableInfo> table_info, std::vector<Tuple> materialized_tuples)
    : TableBase(std::move(table_info))
    , tuples_(std::move(materialized_tuples))
{
}

std::unique_ptr<TableIterator> View::getIterator() const
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
    , current_tuple_(nullptr)
{
}

TableIterator::TableIterator(const View& view)
    : table_(view)
    , current_index_(0)
    , current_row_id_(0)
    , is_view_(true)
    , current_tuple_(nullptr)
{
}

bool TableIterator::hasNext() const
{
    if (is_view_) {
        const View& view = dynamic_cast<const View&>(table_);
        return current_index_ < view.getRowCount();
    }
    const Table& table = static_cast<const Table&>(table_);
    return current_index_ < table.getRowCount();
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
    current_row_id_ = current_index_;
    
    // Create a new tuple and store it in unique_ptr
    current_tuple_ = std::make_unique<Tuple>(table.getTuple(current_index_++));
    return *current_tuple_;
}

void TableIterator::reset()
{
    current_index_ = 0;
    current_row_id_ = 0;
    current_tuple_.reset();
}

} // namespace velodb
