#include "catalog/table.hpp"

#include "catalog/column.hpp"
#include "common/exception.hpp"
#include "common/fmt.hpp"

#include <fmt/core.h>

#include <stdexcept>
#include <utility>

namespace velodb {

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
    std::string result = fmt::format("Table: {}\n", getName());
    result += fmt::format("Schema: {}\n", getSchema());
    for (const auto& tuple : *this) {
        result += fmt::format("{}\n", tuple);
    }
    return result;
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

    const uint64_t new_row_id = row_count_;
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
    return View(std::make_unique<TableInfo>(table_info_->getName(), table_info_->getSchema().cloneUnique()),
                std::move(columns));
}

View Table::viewAs(std::string alias) const
{
    std::vector<ViewColumn> columns;
    columns.reserve(columns_.size());
    for (const auto& column : columns_) {
        columns.emplace_back(column.view());
    }
    return View(std::make_unique<TableInfo>(std::move(alias), table_info_->getSchema().cloneUnique()),
                std::move(columns));
}

void Table::insertRow(std::vector<Value>&& values)
{
    if (values.size() != table_info_->getColumnCount()) {
        VELODB_THROW(CatalogError, "Value count mismatch with schema");
    }

    const uint64_t new_row_id = row_count_;
    ensureColumnCapacity(row_count_ + 1);

    // Move each value into its respective column
    for (size_t col_idx = 0; col_idx < values.size(); ++col_idx) {
        columns_[col_idx][new_row_id] = std::move(values[col_idx]);
    }

    ++row_count_;
}

View Table::slice(size_t start_row, size_t end_row) const
{
    if (start_row > end_row || end_row > row_count_) {
        VELODB_THROW(CatalogError, "Invalid slice range");
    }

    std::vector<ViewColumn> sliced_columns;
    sliced_columns.reserve(columns_.size());

    for (const auto& column : columns_) {
        // Create a slice view of each column
        sliced_columns.emplace_back(column.getType(), column.getName(), column.getValues(), start_row, end_row);
    }

    return View(std::make_unique<TableInfo>(table_info_->getName() + "_slice", table_info_->getSchema().cloneUnique()),
                std::move(sliced_columns));
}

View Table::indices(const std::vector<size_t>& indices) const
{
    for (size_t idx : indices) {
        if (idx >= row_count_) {
            VELODB_THROW(CatalogError, "Index out of range for table");
        }
    }

    std::vector<ViewColumn> indexed_columns;
    indexed_columns.reserve(columns_.size());

    for (const auto& column : columns_) {
        // Create an indices view of each column
        indexed_columns.emplace_back(column.getType(), column.getName(), column.getValues(), indices);
    }

    return View(
        std::make_unique<TableInfo>(table_info_->getName() + "_indexed", table_info_->getSchema().cloneUnique()),
        std::move(indexed_columns));
}

View Table::filterRows(std::function<bool(const ViewTuple&)> predicate) const
{
    std::vector<size_t> matching_indices;

    // Iterate through all rows and collect indices that match the predicate
    for (size_t i = 0; i < row_count_; ++i) {
        ViewTuple tuple(*this, i);
        if (predicate(tuple)) {
            matching_indices.push_back(i);
        }
    }

    return indices(matching_indices);
}

ViewColumn Table::getColumn(const std::string& name) const
{
    size_t column_index = table_info_->getSchema().getColumnIndex(name);
    return getColumn(column_index);
}

ViewColumn Table::getColumn(size_t column_index) const
{
    if (column_index >= columns_.size()) {
        VELODB_THROW(CatalogError, "Column index out of range");
    }
    return columns_[column_index].view();
}

// Efficient column-based access for late materialization
const Value& Table::getValue(uint64_t row_id, size_t column_index) const
{
    if (row_id >= row_count_) {
        VELODB_THROW(CatalogError, "Row ID out of range");
    }
    if (column_index >= columns_.size()) {
        VELODB_THROW(CatalogError, "Column index out of range");
    }

    return columns_[column_index].get(row_id);
}

// View implementation - Column-based storage
View::View(std::unique_ptr<TableInfo> table_info, std::vector<ViewColumn> columns)
    : TableBase(std::move(table_info))
    , columns_(std::move(columns))
    , row_count_(0)
{
    // Determine row count from the first column (all columns should have same
    // size)
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

ViewColumn View::getColumn(size_t column_index) const
{
    if (column_index >= columns_.size()) {
        VELODB_THROW(CatalogError, "Column index out of range");
    }
    return columns_[column_index].view();
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
        columns.push_back(column.view());
    }
    return { std::make_unique<TableInfo>(table_info_->getName(), table_info_->getSchema().cloneUnique()),
             std::move(columns) };
}

View View::viewAs(std::string alias) const
{
    std::vector<ViewColumn> columns;
    columns.reserve(columns_.size());
    for (const auto& column : columns_) {
        columns.push_back(column.view());
    }
    return View(std::make_unique<TableInfo>(std::move(alias), table_info_->getSchema().cloneUnique()),
                std::move(columns));
}

View View::slice(size_t start_row, size_t end_row) const
{
    if (start_row > end_row || end_row > row_count_) {
        VELODB_THROW(CatalogError, "Invalid slice range");
    }

    std::vector<ViewColumn> sliced_columns;
    sliced_columns.reserve(columns_.size());

    for (const auto& column : columns_) {
        sliced_columns.emplace_back(column.slice(start_row, end_row));
    }

    return View(std::make_unique<TableInfo>(table_info_->getName() + "_slice", table_info_->getSchema().cloneUnique()),
                std::move(sliced_columns));
}

View View::indices(const std::vector<size_t>& indices) const
{
    for (size_t idx : indices) {
        if (idx >= row_count_) {
            VELODB_THROW(CatalogError, "Index out of range for view");
        }
    }

    std::vector<ViewColumn> indexed_columns;
    indexed_columns.reserve(columns_.size());

    for (const auto& column : columns_) {
        indexed_columns.emplace_back(column.indices(indices));
    }

    return View(
        std::make_unique<TableInfo>(table_info_->getName() + "_indexed", table_info_->getSchema().cloneUnique()),
        std::move(indexed_columns));
}

View View::filterRows(std::function<bool(const ViewTuple&)> predicate) const
{
    std::vector<size_t> matching_indices;

    // Iterate through all rows and collect indices that match the predicate
    for (size_t i = 0; i < row_count_; ++i) {
        ViewTuple tuple(*this, i);
        if (predicate(tuple)) {
            matching_indices.push_back(i);
        }
    }

    return indices(matching_indices);
}

// Column-based access methods
const Value& View::getValue(uint64_t row_id, size_t column_index) const
{
    if (row_id >= row_count_) {
        VELODB_THROW(CatalogError, "Row ID out of range");
    }
    if (column_index >= columns_.size()) {
        VELODB_THROW(CatalogError, "Column index out of range");
    }

    return columns_[column_index].get(row_id);
}

// TableIterator implementation
TableIterator::TableIterator(const Table& table, uint64_t row_id)
    : table_(table)
    , current_tuple_(table, row_id)
{
}

TableIterator::TableIterator(const View& view, uint64_t row_id)
    : table_(view)
    , current_tuple_(view, row_id)
{
}

bool TableIterator::operator==(const TableIterator& other) const
{
    return current_tuple_ == other.current_tuple_;
}

bool TableIterator::operator!=(const TableIterator& other) const
{
    return !(*this == other);
}

TableIterator& TableIterator::operator++()
{
    if (current_tuple_.row_id_ >= table_.getRowCount()) {
        current_tuple_.row_id_ = table_.getRowCount();
    } else {
        ++current_tuple_.row_id_;
    }
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
    return current_tuple_;
}

const Tuple* TableIterator::operator->() const
{
    return &current_tuple_;
}

} // namespace velodb
