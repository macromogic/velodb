#include "catalog/row_batch.hpp"

#include "common/exception.hpp"
#include "cuda/sort.hpp"
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

void RowBatch::addRows(const RowBatch& other)
{
    VELODB_ASSERT_MSG(columns_.size() == other.getColumnCount(), "Number of columns must match");
    size_t num_columns = columns_.size();
    for (size_t i = 0; i < num_columns; i++) {
        columns_[i].appendMultiple(other.columns_[i]);
    }
    num_rows_ += other.num_rows_;
}

void RowBatch::addFilteredRows(const RowBatch& other, const Column& mask)
{
    VELODB_ASSERT_MSG(other.getRowCount() == mask.size(), "Mask size must match the number of rows in the other batch");
    VELODB_ASSERT_MSG(columns_.size() == other.getColumnCount(), "Column count must match between batches");
    size_t num_columns = columns_.size();
    for (size_t i = 0; i < num_columns; i++) {
        columns_[i].appendMaskedMultiple(other.columns_[i], mask);
    }
    num_rows_ = columns_.empty() ? 0 : columns_.front().size();
}

RowBatch RowBatch::splitFront(size_t size)
{
    if (size > getRowCount()) {
        size = getRowCount();
    }
    std::vector<Column> new_columns;
    new_columns.reserve(columns_.size());
    for (auto& column : columns_) {
        new_columns.push_back(column.splitFront(size));
    }
    num_rows_ = columns_.empty() ? 0 : columns_.front().size();
    return RowBatch(std::move(new_columns));
}

void RowBatch::sort(const std::vector<size_t>& order_indices,
                    const std::vector<bool>& ascending_flags,
                    size_t min_block_size,
                    bool reverse)
{
    if (order_indices.empty() || num_rows_ == 0) {
        return; // Nothing to sort
    }
    VELODB_ASSERT_MSG(order_indices.size() == ascending_flags.size(),
                      "Order indices and ascending flags size must match");
    auto n_columns = getColumnCount();
    for (size_t index : order_indices) {
        VELODB_ASSERT_MSG(index < n_columns, "Order index out of range");
    }

    auto stream_result = StreamPool::instance().acquire();
    if (!stream_result) {
        VELODB_THROW(CatalogError, "Failed to acquire CUDA stream");
    }
    auto sort_stream = std::move(stream_result.value());

    auto n_sort_columns = order_indices.size();
    SortColumn* h_sort_columns = new SortColumn[n_sort_columns];
    for (size_t i = 0; i < n_sort_columns; ++i) {
        size_t col_idx = order_indices[i];
        auto& col = columns_[col_idx];
        std::visit(
            [&](auto&& arg) {
                using DT = typename std::decay_t<decltype(arg)>::DType;
                h_sort_columns[i] = { arg.data(), dTypeId<DT>, ascending_flags[i] };
            },
            col.data_source_);
    }
    SortColumn* d_sort_columns;
    CHECKED_CALL_THROW(cudaMalloc(&d_sort_columns, n_sort_columns * sizeof(SortColumn)));
    CHECKED_CALL_THROW(
        cudaMemcpy(d_sort_columns, h_sort_columns, n_sort_columns * sizeof(SortColumn), cudaMemcpyHostToDevice));
    delete[] h_sort_columns;

    auto* sort_idx = initializeIndices(num_rows_, sort_stream->get());
    sortIndices(sort_idx, num_rows_, d_sort_columns, n_sort_columns, sort_stream->get(), min_block_size, reverse);
    sort_stream->synchronize();
    // TODO: make this non-blocking by using events and stream dependencies
    for (size_t i = 0; i < n_columns; ++i) {
        columns_[i].reorder(sort_idx);
    }

    if (sort_idx != nullptr) {
        cudaFree(sort_idx);
    }
    cudaFree(d_sort_columns);
}

BatchIterator RowBatch::begin() const
{
    return BatchIterator(*this);
}

BatchIterator RowBatch::end() const
{
    return BatchIterator(*this, num_rows_);
}

RowBatch RowBatch::createBuffered(const Schema& schema, size_t initial_capacity, DataLocation location)
{
    std::vector<Column> columns;
    size_t num_columns = schema.getColumnCount();
    columns.reserve(num_columns);
    for (size_t i = 0; i < num_columns; ++i) {
        const auto& col_info = schema.getColumnInfo(i);
        columns.emplace_back(col_info.getType().cloneUnique(), initial_capacity, location);
    }
    return RowBatch(std::move(columns));
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
