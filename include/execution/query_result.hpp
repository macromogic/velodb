#pragma once

#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include "catalog/tuple.hpp"
#include "common/copy_traits.hpp"

#include <optional>

namespace velodb {

class QueryResultIterator; // Forward declaration

class QueryResult : public NonCopyable {
public:
    explicit QueryResult(Schema schema);
    QueryResult(QueryResult&&) noexcept = default;
    QueryResult& operator=(QueryResult&&) noexcept = default;
    ~QueryResult() = default;

    void append(RowBatch batch);

    Value getValue(size_t row, size_t column) const;

    // Schema and basic info
    const Schema& getSchema() const { return schema_; }
    size_t getRowCount() const { return row_count_; }
    bool isEmpty() const { return row_count_ == 0; }

    std::string toString() const;

    // Iterator support for range-based for loops
    QueryResultIterator begin() const;
    QueryResultIterator end() const;

private:
    Schema schema_;
    std::vector<RowBatch> batches_;
    std::vector<size_t> row_offsets_;

    size_t row_count_; // Current number of rows

    friend class QueryResultIterator; // Allow iterator access to private members
};

// Iterator class for QueryResult
class QueryResultIterator {
public:
    explicit QueryResultIterator(const QueryResult& result, size_t view_idx = 0, size_t row_id = 0);
    ~QueryResultIterator() = default;

    bool operator==(const QueryResultIterator& other) const;
    bool operator!=(const QueryResultIterator& other) const;

    QueryResultIterator& operator++(); // Pre-increment
    QueryResultIterator operator++(int); // Post-increment

    // Dereference operators - returns a tuple representation
    const ViewTuple& operator*() const;
    const ViewTuple* operator->() const;

private:
    const QueryResult& result_;
    size_t view_idx_;
    ViewTuple current_tuple_;
};

} // namespace velodb
