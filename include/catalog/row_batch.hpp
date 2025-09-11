#pragma once

#include "catalog/column.hpp"
#include "catalog/tuple.hpp"
#include "data/value.hpp"

namespace velodb {

// Forward declarations
class BatchIterator;
class Tuple;

class RowBatch {
public:
    RowBatch() = default;

    explicit RowBatch(std::vector<Column> columns)
        : columns_(std::move(columns))
    {
    }

    void addColumn(Column column) { columns_.push_back(std::move(column)); }
    Column& getColumn(size_t index);
    size_t getColumnCount() const { return columns_.size(); }
    size_t getRowCount() const;
    Value getValue(size_t row_id, size_t column_index) const;
    void to(DataLocation location);
    void compact(std::vector<Column>& destination, size_t mask_index);

    BatchIterator begin() const;
    BatchIterator end() const;

private:
    std::vector<Column> columns_;
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
