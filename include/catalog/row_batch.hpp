#pragma once

#include "catalog/column.hpp"
#include "catalog/tuple.hpp"
#include "data/value.hpp"

namespace velodb {

// Forward declarations
class BatchIterator;
class Tuple;
class Schema;

class RowBatch : private NonCopyable {
public:
    RowBatch() = default;
    RowBatch(RowBatch&& other) = default;
    RowBatch& operator=(RowBatch&& other) = default;

    void addColumn(Column&& column);
    Column& getColumn(size_t index);
    size_t getColumnCount() const { return columns_.size(); }
    size_t getRowCount() const { return num_rows_; }
    Value getValue(size_t row_id, size_t column_index) const;
    void to(DataLocation location);
    void addRows(const RowBatch& other);
    void addFilteredRows(const RowBatch& other, const Column& mask);
    RowBatch splitFront(size_t size);

    BatchIterator begin() const;
    BatchIterator end() const;

    static RowBatch createBuffered(const Schema& schema, size_t initial_capacity, DataLocation location);

private:
    std::vector<Column> columns_;
    size_t num_rows_;

    explicit RowBatch(std::vector<Column> columns)
        : columns_(std::move(columns))
        , num_rows_(columns_.empty() ? 0 : columns_.front().size())
    {
    }
};

// Iterator for table scanning
class BatchIterator {
public:
    explicit BatchIterator(const RowBatch& batch, size_t row_id = 0);
    ~BatchIterator() = default;

    bool operator==(const BatchIterator& other) const;
    bool operator!=(const BatchIterator& other) const;

    BatchIterator& operator++();
    BatchIterator operator++(int);
    const Tuple& operator*() const;
    const Tuple* operator->() const;

private:
    const RowBatch& batch_;
    ViewTuple current_tuple_;
};

} // namespace velodb
