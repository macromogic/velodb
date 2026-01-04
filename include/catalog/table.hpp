#pragma once

#include "catalog/schema.hpp"
#include "common/copy_traits.hpp"
#include "data/value.hpp"

#include <string>
#include <vector>

namespace velodb {

// Forward declarations
class BatchIterator;
class TableBuilder;
class RowBatch;
class Column;

// Concrete table implementation
class Table : private NonCopyable {
public:
    Table(std::string name, Schema schema);
    ~Table() = default;

    // Move constructor and assignment
    Table(Table&& other) noexcept = default;
    Table& operator=(Table&& other) noexcept = default;

    const std::string& getName() const { return name_; }
    size_t getRowCount() const { return row_count_; }
    const Schema& getSchema() const { return schema_; }
    size_t getColumnIndex(const std::string& name) const;
    const std::string& getColumnName(size_t index) const;
    const DataType& getColumnType(const std::string& name) const;
    const DataType& getColumnType(size_t index) const;
    size_t getColumnCount() const { return columns_.size(); }

    const Column& getColumn(const std::string& name) const;
    const Column& getColumn(size_t column_index) const;
    bool hasColumn(const std::string& name) const;

    const Value getValue(size_t row_id, size_t column_index) const;

    std::string toString() const;

    RowBatch slice(size_t begin, size_t end) const;

private:
    std::string name_;
    Schema schema_;
    std::vector<Column> columns_;
    size_t row_count_; // Current number of rows

    // Helper methods
    void initializeColumns();

    friend class BatchIterator;
    friend class TableBuilder;
};

} // namespace velodb
