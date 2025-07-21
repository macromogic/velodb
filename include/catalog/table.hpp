#pragma once

#include "schema.hpp"
#include "types/value.hpp"
#include "common/non_copyable.hpp"
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace velodb {

// Forward declarations
class Tuple;
class TableIterator;

// Row ID type for late materialization
using RowId = uint64_t;
constexpr RowId INVALID_ROW_ID = UINT64_MAX;

class Tuple {
public:
    explicit Tuple(const Schema& schema);
    Tuple(const Schema& schema, std::vector<Value> values);
    ~Tuple() = default;

    // Copy and move constructors
    Tuple(const Tuple& other) = default;
    Tuple(Tuple&& other) noexcept = default;
    Tuple& operator=(const Tuple& other) = default;
    Tuple& operator=(Tuple&& other) noexcept = default;

    [[nodiscard]] const Value& getValue(size_t column_index) const;
    [[nodiscard]] const Value& getValue(const std::string& column_name) const;
    void setValue(size_t column_index, const Value& value);
    void setValue(size_t column_index, Value&& value);
    void setValue(const std::string& column_name, const Value& value);
    void setValue(const std::string& column_name, Value&& value);

    [[nodiscard]] const Schema& getSchema() const { return schema_.get(); }
    [[nodiscard]] size_t getColumnCount() const { return values_.size(); }

    [[nodiscard]] std::string toString() const;

private:
    std::reference_wrapper<const Schema> schema_;
    std::vector<Value> values_;
};

class TableInfo : private NonCopyable {
public:
    TableInfo(std::string name, std::unique_ptr<Schema> schema);
    ~TableInfo() = default;

    // Move constructor and assignment
    TableInfo(TableInfo&& other) noexcept = default;
    TableInfo& operator=(TableInfo&& other) noexcept = default;

    [[nodiscard]] const std::string& getName() const { return name_; }
    [[nodiscard]] const Schema& getSchema() const { return *schema_; }
    [[nodiscard]] size_t getColumnCount() const { return schema_->getColumnCount(); }

private:
    std::string name_;
    std::unique_ptr<Schema> schema_;
};

// Abstract base class for tables and views
class TableBase : private NonCopyable {
public:
    explicit TableBase(std::unique_ptr<TableInfo> table_info);
    virtual ~TableBase() = default;

    [[nodiscard]] const std::string& getName() const { return table_info_->getName(); }
    [[nodiscard]] const Schema& getSchema() const { return table_info_->getSchema(); }
    [[nodiscard]] const TableInfo& getTableInfo() const { return *table_info_; }

    // Pure virtual methods for table operations
    virtual std::unique_ptr<TableIterator> getIterator() const = 0;
    [[nodiscard]] virtual size_t getRowCount() const = 0;
    [[nodiscard]] virtual bool isView() const = 0;

protected:
    std::unique_ptr<TableInfo> table_info_;
};

// Concrete table implementation with column-based storage
class Table : public TableBase {
public:
    explicit Table(std::unique_ptr<TableInfo> table_info);
    ~Table() override = default;

    // TableBase interface
    std::unique_ptr<TableIterator> getIterator() const override;
    [[nodiscard]] size_t getRowCount() const override { return row_count_; }
    [[nodiscard]] bool isView() const override { return false; }

    // Primary column-based insertion methods
    void insertRow(const std::vector<Value>& values);
    void insertRow(std::vector<Value>&& values);
    void insertBatchRows(const std::vector<std::vector<Value>>& rows);
    
    // Legacy Tuple-based methods for backward compatibility
    void insertTuple(const Tuple& tuple);
    void insertTuple(Tuple&& tuple);
    [[nodiscard]] Tuple getTuple(RowId row_id) const; // Reconstructs tuple from columns
    void insertBatch(const std::vector<Tuple>& tuples);

    // Efficient column-based access for late materialization
    [[nodiscard]] Value getValue(RowId row_id, size_t column_index) const;
    [[nodiscard]] std::vector<Value> getValues(RowId row_id, const std::vector<size_t>& column_indices) const;
    [[nodiscard]] const ValueVector& getColumn(size_t column_index) const;
    [[nodiscard]] ValueVector& getColumn(size_t column_index);
    [[nodiscard]] std::vector<Value> getColumnValues(size_t column_index, const std::vector<RowId>& row_ids) const;
    
    // Efficient batch column access
    [[nodiscard]] std::vector<ValueVector> getColumns(const std::vector<size_t>& column_indices) const;
    
    // Row iteration without materializing tuples
    [[nodiscard]] std::vector<RowId> getAllRowIds() const;
    [[nodiscard]] std::vector<RowId> getValidRowIds() const; // Excludes deleted rows

private:
    // Column-based storage: each column is stored as a separate vector
    std::vector<ValueVector> columns_;
    size_t row_count_; // Current number of rows (including deleted)
    
    // Helper methods
    void ensureColumnCapacity(size_t new_row_count);
    void initializeColumns();
    void insertRowInternal(const std::vector<Value>& values);

    friend class TableIterator; // Allow iterator access to private members
};

// View implementation (for derived tables from queries)
class View : public TableBase {
public:
    View(std::unique_ptr<TableInfo> table_info, std::vector<Tuple> materialized_tuples);
    ~View() override = default;

    // TableBase interface
    std::unique_ptr<TableIterator> getIterator() const override;
    [[nodiscard]] size_t getRowCount() const override { return tuples_.size(); }
    [[nodiscard]] bool isView() const override { return true; }

    [[nodiscard]] const Tuple& getTuple(size_t index) const;

private:
    std::vector<Tuple> tuples_;
};

// Iterator for table scanning
class TableIterator {
public:
    explicit TableIterator(const Table& table);
    explicit TableIterator(const View& view);
    ~TableIterator() = default;

    [[nodiscard]] bool hasNext() const;
    const Tuple& next();
    [[nodiscard]] RowId getCurrentRowId() const { return current_row_id_; }
    void reset();

private:
    const TableBase& table_;
    size_t current_index_;
    RowId current_row_id_;
    bool is_view_;
    mutable std::unique_ptr<Tuple> current_tuple_; // Store tuple as unique_ptr
};

} // namespace velodb
