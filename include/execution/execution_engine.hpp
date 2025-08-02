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
