#pragma once

#include "catalog/catalog.hpp"
#include "expression/expression.hpp"
#include "operator/operator.hpp"
#include "planner/planner.hpp"
#include <memory>
#include <vector>

// Forward declarations for SQL parser
namespace hsql {
struct SQLStatement;
struct SelectStatement;
}

namespace velodb {

// Query execution result - Column-based storage
class QueryResult : public NonCopyable {
public:
    explicit QueryResult(std::unique_ptr<Schema> schema);
    ~QueryResult() = default;

    // Column-based insertion methods
    void addRow(const std::vector<Value>& values);
    void addRow(std::vector<Value>&& values);
    void addBatchRows(const std::vector<std::vector<Value>>& rows);

    // Schema and basic info
    const Schema& getSchema() const { return *schema_; }
    size_t getRowCount() const { return row_count_; }
    bool isEmpty() const { return row_count_ == 0; }

    // Column-based access methods
    Value getValue(RowId row_id, size_t column_index) const;
    std::vector<Value> getValues(RowId row_id, const std::vector<size_t>& column_indices) const;
    const ValueVector& getColumn(size_t column_index) const;
    std::vector<Value> getColumnValues(size_t column_index, const std::vector<RowId>& row_ids) const;
    std::vector<ValueVector> getColumns(const std::vector<size_t>& column_indices) const;

    // Row ID management
    std::vector<RowId> getAllRowIds() const;

    std::string toString() const;

    // Conversion to View for catalog integration
    // std::unique_ptr<View> toView(const std::string& view_name) const;

private:
    std::unique_ptr<Schema> schema_;

    // Column-based storage: each column is stored as a separate vector
    std::vector<ValueVector> columns_;
    size_t row_count_; // Current number of rows

    // Helper methods
    void initializeColumns();
    void ensureColumnCapacity(size_t new_row_count);
    void insertRowInternal(const std::vector<Value>& values);
};

// Main execution engine
class ExecutionEngine {
public:
    explicit ExecutionEngine(Catalog& catalog);
    ~ExecutionEngine() = default;

    // Delete copy constructor and assignment
    ExecutionEngine(const ExecutionEngine&) = delete;
    ExecutionEngine& operator=(const ExecutionEngine&) = delete;

    // Main execution interface
    Result<View> executeQuery(const std::string& sql);
    Result<View> executeStatement(const hsql::SQLStatement* statement);
    Result<View> executeSelect(const hsql::SelectStatement* select_stmt);

    // Plan execution
    Result<View> executePlan(std::unique_ptr<AbstractPlanNode> plan);

    size_t getLastExecutionRowCount() const { return last_execution_row_count_; }
    double getLastExecutionTimeMs() const { return last_execution_time_ms_; }

private:
    // Helper methods
    std::unique_ptr<AbstractOperator> createOperatorTree(const AbstractPlanNode& plan_node);

    Catalog& catalog_;
    std::unique_ptr<QueryPlanner> planner_;
    std::unique_ptr<ExecutionContext> context_;

    size_t last_execution_row_count_ { 0 };
    double last_execution_time_ms_ { 0.0 };
};

// Utility class for query execution statistics
class ExecutionStats {
public:
    ExecutionStats() { }

    void setRowsProcessed(size_t rows) { rows_processed_ = rows; }
    void setExecutionTime(double time_ms) { execution_time_ms_ = time_ms; }

    size_t getRowsProcessed() const { return rows_processed_; }
    double getExecutionTime() const { return execution_time_ms_; }

    std::string toString() const;

private:
    size_t rows_processed_ { 0 };
    double execution_time_ms_ { 0.0 };
};

} // namespace velodb
