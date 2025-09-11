#include "catalog/row_batch.hpp"

#include "common/exception.hpp"
#include "cuda/stream_pool.hpp"

namespace velodb {

Column& RowBatch::getColumn(size_t index)
{
    if (index >= columns_.size()) {
        VELODB_THROW(CatalogError, "Column index out of range");
    }
    return columns_[index];
}

size_t RowBatch::getRowCount() const
{
    if (columns_.empty()) {
        return 0;
    }
    return columns_.front().size();
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
    // TODO: Blocking data movement here. May redesign for streaming
    auto stream_result = StreamPool::instance().acquire();
    if (!stream_result) {
        VELODB_THROW(CatalogError, "Failed to acquire CUDA stream");
    }
    auto join_stream = std::move(stream_result.value());
    for (auto& column : columns_) {
        auto event_result = column.to(location);
        if (!event_result) {
            // Handle error (e.g., throw an exception or log the error)
            VELODB_THROW(CatalogError, "Failed to move column to the specified location");
        }
        join_stream->recordEvent(*event_result.value());
    }
    join_stream->synchronize();
}

void RowBatch::compact(std::vector<Column>& destination, size_t mask_index)
{
    VELODB_ASSERT_MSG(columns_.size() == destination.size(), "Number of columns mismatch");
    auto& mask_column = columns_[mask_index];
    VELODB_ASSERT_MSG(mask_column.getType().isBoolean(), "Mask column must be boolean");
    size_t num_columns = columns_.size();
    for (size_t i = 0; i < num_columns; i++) {
        destination[i].appendMultiple(columns_[i], mask_column);
    }
}

BatchIterator RowBatch::begin() const
{
    return BatchIterator(*this);
}

BatchIterator RowBatch::end() const
{
    return BatchIterator(*this, getRowCount());
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
