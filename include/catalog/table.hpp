#pragma once

#include "schema.hpp"
#include "types/value.hpp"
#include "common/copy_traits.hpp"
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace velodb {

// Forward declarations
class Tuple;
class View;
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

    const Value& getValue(size_t column_index) const;
    const Value& getValue(const std::string& column_name) const;
    void setValue(size_t column_index, const Value& value);
    void setValue(size_t column_index, Value&& value);
    void setValue(const std::string& column_name, const Value& value);
    void setValue(const std::string& column_name, Value&& value);

    const Schema& getSchema() const { return schema_.get(); }
    size_t getColumnCount() const { return values_.size(); }

    std::string toString() const;

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

    const std::string& getName() const { return name_; }
    const Schema& getSchema() const { return *schema_; }
    size_t getColumnCount() const { return schema_->getColumnCount(); }
    void addColumnInfo(ColumnInfo column);

private:
    std::string name_;
    std::unique_ptr<Schema> schema_;
};

// Abstract base class for tables and views
class TableBase : private NonCopyable {
public:
    explicit TableBase(std::unique_ptr<TableInfo> table_info);

    // Explicitly allow move semantics for abstract base class
    TableBase(TableBase&& other) = default;
    TableBase& operator=(TableBase&& other) = default;

    virtual ~TableBase() = default;

    const std::string& getName() const { return table_info_->getName(); }
    const Schema& getSchema() const { return table_info_->getSchema(); }
    const TableInfo& getTableInfo() const { return *table_info_; }

    // Pure virtual methods for table operations
    virtual TableIterator begin() const = 0;
    virtual TableIterator end() const = 0;
    virtual size_t getRowCount() const = 0;
    virtual bool isView() const = 0;
    virtual View view() const = 0;
    virtual View viewAs(std::string alias) const = 0;

    std::string toString() const;

protected:
    std::unique_ptr<TableInfo> table_info_;
};

// Concrete table implementation
class Table : public TableBase {
public:
    explicit Table(std::unique_ptr<TableInfo> table_info);
    ~Table() override = default;

    // TableBase interface
    TableIterator begin() const override;
    TableIterator end() const override;
    size_t getRowCount() const override { return row_count_; }
    bool isView() const override { return false; }
    View view() const override;
    View viewAs(std::string alias) const override;

    // Primary column-based insertion methods
    void insertRow(const std::vector<Value>& values);
    void insertRow(std::vector<Value>&& values);
    void insertBatchRows(const std::vector<std::vector<Value>>& rows);

    // Efficient column-based access for late materialization
    Value getValue(RowId row_id, size_t column_index) const;
    std::vector<Value> getValues(RowId row_id, const std::vector<size_t>& column_indices) const;
    std::vector<Value> getColumnValues(size_t column_index, const std::vector<RowId>& row_ids) const;

private:
    // Column-based storage: each column is stored as a separate vector
    std::vector<ValueColumn> columns_;
    size_t row_count_; // Current number of rows (including deleted)

    // Helper methods
    void ensureColumnCapacity(size_t new_row_count);
    void initializeColumns();

    friend class TableIterator; // Allow iterator access to private members
};

// View implementation (for derived tables from queries)
class View : public TableBase {
public:
    View(std::unique_ptr<TableInfo> table_info, std::vector<ViewColumn> columns);
    explicit View(std::string name); // For empty view creation
    View(View&& other) noexcept = default;
    View& operator=(View&& other) noexcept = default;
    ~View() override = default;

    // TableBase interface
    TableIterator begin() const override;
    TableIterator end() const override;
    size_t getRowCount() const override { return row_count_; }
    bool isView() const override { return true; }
    View view() const override;
    View viewAs(std::string alias) const override;

    void addColumn(ViewColumn column);
    ViewColumn getColumn(const std::string& name) const;
    // Column-based access methods (similar to Table)
    Value getValue(RowId row_id, size_t column_index) const;
    std::vector<Value> getValues(RowId row_id, const std::vector<size_t>& column_indices) const;
    std::vector<Value> getColumnValues(size_t column_index, const std::vector<RowId>& row_ids) const;

private:
    // Column-based storage: each column is stored as a separate vector
    std::vector<ViewColumn> columns_;
    size_t row_count_; // Current number of rows
};

// Iterator for table scanning
class TableIterator {
public:
    explicit TableIterator(const Table& table, RowId row_id = 0);
    explicit TableIterator(const View& view, RowId row_id = 0);
    ~TableIterator() = default;

    bool operator==(const TableIterator& other) const;
    bool operator!=(const TableIterator& other) const;

    TableIterator& operator++(); // Pre-increment
    TableIterator operator++(int); // Post-increment
    const Tuple& operator*() const; // Dereference to get current tuple
    const Tuple* operator->() const; // Pointer to current tuple

private:
    const TableBase& table_;
    RowId current_row_id_;
    bool is_view_;
    std::shared_ptr<Tuple> current_tuple_; // Store tuple as shared_ptr

    void fetchCurrentTuple(); // Helper to fetch current tuple based on row ID
};

} // namespace velodb
