#include "catalog/table.hpp"
#include "catalog/column.hpp"
#include "common/exception.hpp"
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
        values_[i] = Value::createNull(schema.getColumnInfo(i).getType().getTypeId());
    }
}

Tuple::Tuple(const Schema& schema, std::vector<Value> values)
    : schema_(std::cref(schema))
    , values_(std::move(values))
{
    if (values_.size() != schema.getColumnCount()) {
        VELODB_THROW(CatalogError, "Value count mismatch with schema");
    }
}

const Value& Tuple::getValue(size_t column_index) const
{
    if (column_index >= values_.size()) {
        VELODB_THROW(CatalogError, "Column index out of range");
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
        VELODB_THROW(CatalogError, "Column index out of range");
    }
    values_[column_index] = value;
}

void Tuple::setValue(size_t column_index, Value&& value)
{
    if (column_index >= values_.size()) {
        VELODB_THROW(CatalogError, "Column index out of range");
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

void TableInfo::addColumnInfo(ColumnInfo column)
{
    if (schema_->hasColumn(column.getName())) {
        VELODB_THROW(CatalogError, "Column already exists: " + column.getName());
    }
    schema_->addColumnInfo(std::move(column));
}

// TableBase implementation
TableBase::TableBase(std::unique_ptr<TableInfo> table_info)
    : table_info_(std::move(table_info))
{
}

std::string TableBase::toString() const
{
    std::stringstream ss;
    ss << "Table: " << getName() << "\n";
    ss << "Schema: " << getSchema().toString() << "\n";
    for (const auto& tuple : *this) {
        ss << tuple.toString() << "\n";
    }
    return ss.str();
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
    auto& schema = table_info_->getSchema();
    columns_.reserve(column_count);
    for (auto& info : schema) {
        columns_.emplace_back(info);
    }
}

void Table::ensureColumnCapacity(size_t new_row_count)
{
    for (auto& column : columns_) {
        const size_t old_size = column.size();
        if (new_row_count > old_size) {
            // Resize columns to accommodate new rows
            // Fill with null values for the appropriate type
            column.resize(new_row_count);
            if (old_size > 0) {
                // Use the type from existing values in the column
                for (size_t i = old_size; i < new_row_count; ++i) {
                    column[i] = Value::createNull(column[0].getTypeId());
                }
            }
        }
    }
}

TableIterator Table::begin() const
{
    return TableIterator(*this);
}

TableIterator Table::end() const
{
    return TableIterator(*this, row_count_);
}

// Primary column-based insertion methods
void Table::insertRow(const std::vector<Value>& values)
{
    if (values.size() != table_info_->getColumnCount()) {
        VELODB_THROW(CatalogError, "Value count mismatch with schema");
    }

    const RowId new_row_id = row_count_;
    ensureColumnCapacity(row_count_ + 1);

    // Insert each value into its respective column
    for (size_t col_idx = 0; col_idx < values.size(); ++col_idx) {
        columns_[col_idx][new_row_id] = values[col_idx];
    }

    ++row_count_;
}

View Table::view() const
{
    std::vector<ViewColumn> columns;
    columns.reserve(columns_.size());
    for (const auto& column : columns_) {
        columns.emplace_back(column.view());
    }
    return View(std::make_unique<TableInfo>(table_info_->getName(), table_info_->getSchema().cloneUnique()), std::move(columns));
}

View Table::viewAs(std::string alias) const
{
    std::vector<ViewColumn> columns;
    columns.reserve(columns_.size());
    for (const auto& column : columns_) {
        columns.emplace_back(column.view());
    }
    return View(std::make_unique<TableInfo>(std::move(alias), table_info_->getSchema().cloneUnique()), std::move(columns));
}

void Table::insertRow(std::vector<Value>&& values)
{
    if (values.size() != table_info_->getColumnCount()) {
        VELODB_THROW(CatalogError, "Value count mismatch with schema");
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
            VELODB_THROW(CatalogError, "Value count mismatch with schema");
        }

        const RowId row_id = old_row_count + row_idx;
        for (size_t col_idx = 0; col_idx < values.size(); ++col_idx) {
            columns_[col_idx][row_id] = values[col_idx];
        }
    }

    row_count_ = new_row_count;
}

Value Table::getValue(RowId row_id, size_t column_index) const
{
    if (row_id >= row_count_) {
        VELODB_THROW(CatalogError, "Row ID out of range");
    }
    if (column_index >= columns_.size()) {
        VELODB_THROW(CatalogError, "Column index out of range");
    }

    return columns_[column_index].get(row_id);
}

std::vector<Value> Table::getValues(RowId row_id, const std::vector<size_t>& column_indices) const
{
    if (row_id >= row_count_) {
        VELODB_THROW(CatalogError, "Row ID out of range");
    }

    std::vector<Value> values;
    values.reserve(column_indices.size());

    for (size_t const col_idx : column_indices) {
        if (col_idx >= columns_.size()) {
            VELODB_THROW(CatalogError, "Column index out of range");
        }
        values.push_back(columns_[col_idx].get(row_id));
    }
    return values;
}

std::vector<Value> Table::getColumnValues(size_t column_index, const std::vector<RowId>& row_ids) const
{
    if (column_index >= columns_.size()) {
        VELODB_THROW(CatalogError, "Column index out of range");
    }

    std::vector<Value> values;
    values.reserve(row_ids.size());

    for (RowId const row_id : row_ids) {
        if (row_id >= row_count_) {
            VELODB_THROW(CatalogError, "Row ID out of range");
        }
        values.push_back(columns_[column_index].get(row_id));
    }

    return values;
}

// View implementation - Column-based storage
View::View(std::unique_ptr<TableInfo> table_info, std::vector<ViewColumn> columns)
    : TableBase(std::move(table_info))
    , columns_(std::move(columns))
    , row_count_(0)
{
    // Determine row count from the first column (all columns should have same size)
    if (!columns_.empty()) {
        row_count_ = columns_[0].size();

        // Validate all columns have the same size
        for (size_t i = 1; i < columns_.size(); ++i) {
            if (columns_[i].size() != row_count_) {
                VELODB_THROW(CatalogError, "All columns must have the same number of rows");
            }
        }
    }
}

View::View(std::string name)
    : TableBase(std::make_unique<TableInfo>(std::move(name), std::make_unique<Schema>()))
    , columns_()
    , row_count_(0)
{
}

void View::addColumn(ViewColumn column)
{
    if (row_count_ == 0 && columns_.empty()) {
        row_count_ = column.size();
    }
    if (column.size() != row_count_) {
        VELODB_THROW(CatalogError, "New column size must match existing row count");
    }
    table_info_->addColumnInfo({ column.getName(), column.getType().cloneUnique() });
    columns_.push_back(std::move(column));
}

ViewColumn View::getColumn(const std::string& name) const
{
    for (const auto& column : columns_) {
        if (column.getName() == name) {
            return column.view();
        }
    }
    VELODB_THROW(CatalogError, "Column not found: " + name);
}

TableIterator View::begin() const
{
    return TableIterator(*this);
}

TableIterator View::end() const
{
    return TableIterator(*this, row_count_);
}

View View::view() const
{
    std::vector<ViewColumn> columns;
    columns.reserve(columns_.size());
    for (const auto& column : columns_) {
        columns.emplace_back(column.getType(), column.getName(), column.getValues());
    }
    return {
        std::make_unique<TableInfo>(table_info_->getName(), table_info_->getSchema().cloneUnique()),
        std::move(columns)
    };
}

View View::viewAs(std::string alias) const
{
    std::vector<ViewColumn> columns;
    columns.reserve(columns_.size());
    for (const auto& column : columns_) {
        columns.emplace_back(column.getType(), column.getName(), column.getValues());
    }
    return View(std::make_unique<TableInfo>(std::move(alias), table_info_->getSchema().cloneUnique()), std::move(columns));
}

// Column-based access methods
Value View::getValue(RowId row_id, size_t column_index) const
{
    if (row_id >= row_count_) {
        VELODB_THROW(CatalogError, "Row ID out of range");
    }
    if (column_index >= columns_.size()) {
        VELODB_THROW(CatalogError, "Column index out of range");
    }

    return columns_[column_index].get(row_id);
}

std::vector<Value> View::getValues(RowId row_id, const std::vector<size_t>& column_indices) const
{
    if (row_id >= row_count_) {
        VELODB_THROW(CatalogError, "Row ID out of range");
    }

    std::vector<Value> values;
    values.reserve(column_indices.size());

    for (size_t const col_idx : column_indices) {
        if (col_idx >= columns_.size()) {
            VELODB_THROW(CatalogError, "Column index out of range");
        }
        values.push_back(columns_[col_idx].get(row_id));
    }
    return values;
}

std::vector<Value> View::getColumnValues(size_t column_index, const std::vector<RowId>& row_ids) const
{
    if (column_index >= columns_.size()) {
        VELODB_THROW(CatalogError, "Column index out of range");
    }

    std::vector<Value> values;
    values.reserve(row_ids.size());

    for (RowId const row_id : row_ids) {
        if (row_id >= row_count_) {
            VELODB_THROW(CatalogError, "Row ID out of range");
        }
        values.push_back(columns_[column_index].get(row_id));
    }

    return values;
}

// TableIterator implementation
TableIterator::TableIterator(const Table& table, RowId row_id)
    : table_(table)
    , current_row_id_(row_id)
    , is_view_(false)
    , current_tuple_(nullptr)
{
    fetchCurrentTuple();
}

TableIterator::TableIterator(const View& view, RowId row_id)
    : table_(view)
    , current_row_id_(row_id)
    , is_view_(true)
    , current_tuple_(nullptr)
{
    fetchCurrentTuple();
}

bool TableIterator::operator==(const TableIterator& other) const
{
    return &table_ == &other.table_ && current_row_id_ == other.current_row_id_;
}

bool TableIterator::operator!=(const TableIterator& other) const
{
    return !(*this == other);
}

TableIterator& TableIterator::operator++()
{
    if (current_row_id_ >= table_.getRowCount()) {
        current_row_id_ = table_.getRowCount();
        current_tuple_.reset();
    }

    // Move to next row
    ++current_row_id_;
    fetchCurrentTuple();

    return *this;
}

TableIterator TableIterator::operator++(int)
{
    TableIterator temp = *this;
    ++(*this);
    return temp;
}

const Tuple& TableIterator::operator*() const
{
    if (!current_tuple_) {
        VELODB_THROW(CatalogError, "Dereferencing invalid iterator");
    }
    return *current_tuple_;
}

const Tuple* TableIterator::operator->() const
{
    if (!current_tuple_) {
        VELODB_THROW(CatalogError, "Dereferencing invalid iterator");
    }
    return current_tuple_.get();
}

void TableIterator::fetchCurrentTuple()
{
    if (current_row_id_ >= table_.getRowCount()) {
        current_tuple_.reset();
        return;
    }
    if (!current_tuple_) {
        current_tuple_ = std::make_shared<Tuple>(table_.getSchema());
    }

    if (is_view_) {
        const View& view = dynamic_cast<const View&>(table_);
        for (size_t col_idx = 0; col_idx < table_.getSchema().getColumnCount(); ++col_idx) {
            current_tuple_->setValue(col_idx, view.getValue(current_row_id_, col_idx));
        }
    } else {
        const Table& table = static_cast<const Table&>(table_);
        for (size_t col_idx = 0; col_idx < table_.getSchema().getColumnCount(); ++col_idx) {
            current_tuple_->setValue(col_idx, table.getValue(current_row_id_, col_idx));
        }
    }
}

} // namespace velodb
