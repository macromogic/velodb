#include "catalog/row_batch.hpp"

#include "common/exception.hpp"
#include "common/profiler.hpp"
#include "cuda/stream_pool.hpp"

namespace velodb {

void RowBatch::addColumn(Column&& column)
{
    if (columns_.empty()) {
        num_rows_ = column.size();
    } else if (column.size() != num_rows_) {
        VELODB_THROW(CatalogError, "All columns must have the same number of rows");
    }
    columns_.push_back(std::move(column));
}

Column& RowBatch::getColumn(size_t index)
{
    if (index >= columns_.size()) {
        VELODB_THROW(CatalogError, "Column index out of range");
    }
    return columns_[index];
}

const Column& RowBatch::getColumn(size_t index) const
{
    if (index >= columns_.size()) {
        VELODB_THROW(CatalogError, "Column index out of range");
    }
    return columns_[index];
}

void RowBatch::setRowCount(size_t new_row_count)
{
    for (auto& col : columns_) {
        col.setSize(new_row_count);
    }
    num_rows_ = new_row_count;
}

Value RowBatch::getValue(size_t row_id, size_t column_index) const
{
    if (row_id >= getRowCount()) {
        VELODB_THROW(CatalogError, "Row ID out of range");
    }
    if (column_index >= getColumnCount()) {
        VELODB_THROW(CatalogError, "Column index out of range");
    }
    return columns_[column_index].get(row_id);
}

void RowBatch::to(DataLocation location)
{
    // TODO: Blocking data movement here
    for (auto& column : columns_) {
        column.to(location);
    }
}

BatchIterator RowBatch::begin() const
{
    return BatchIterator(*this);
}

BatchIterator RowBatch::end() const
{
    return BatchIterator(*this, num_rows_);
}

void RowBatch::debug() const
{
    for (auto& row : *this) {
        fmt::println("{}", row.toString());
    }
}

void RowBatch::debug(size_t max_rows) const
{
    size_t row_count = 0;
    for (auto& row : *this) {
        fmt::println("{}", row.toString());
        if (++row_count >= max_rows) {
            break;
        }
    }
}

// BatchIterator implementation
BatchIterator::BatchIterator(const RowBatch& batch, size_t row_id)
    : batch_(batch)
    , current_tuple_(batch, row_id)
{
}

bool BatchIterator::operator==(const BatchIterator& other) const
{
    return current_tuple_ == other.current_tuple_;
}

bool BatchIterator::operator!=(const BatchIterator& other) const
{
    return !(*this == other);
}

BatchIterator& BatchIterator::operator++()
{
    if (current_tuple_.row_id_ >= batch_.getRowCount()) {
        current_tuple_.row_id_ = batch_.getRowCount();
    } else {
        ++current_tuple_.row_id_;
    }
    return *this;
}

BatchIterator BatchIterator::operator++(int)
{
    BatchIterator temp = *this;
    operator++();
    return temp;
}

const Tuple& BatchIterator::operator*() const
{
    return current_tuple_;
}

const Tuple* BatchIterator::operator->() const
{
    return &current_tuple_;
}

} // namespace velodb
