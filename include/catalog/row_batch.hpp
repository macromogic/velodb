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
    const Column& getColumn(size_t index) const;
    std::vector<Column>& getColumns() { return columns_; }
    const std::vector<Column>& getColumns() const { return columns_; }
    size_t getColumnCount() const { return columns_.size(); }
    size_t getRowCount() const { return num_rows_; }
    Value getValue(size_t row_id, size_t column_index) const;
    void to(DataLocation location);

    BatchIterator begin() const;
    BatchIterator end() const;

    void debug() const;
    void debug(size_t max_rows) const;

private:
    std::vector<Column> columns_;
    size_t num_rows_ { 0 };

    explicit RowBatch(std::vector<Column> columns)
        : columns_(std::move(columns))
        , num_rows_(columns_.empty() ? 0 : columns_.front().size())
    {
    }
    void setRowCount(size_t row_count);

    friend class AbstractOperator;
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
