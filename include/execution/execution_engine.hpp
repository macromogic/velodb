#pragma once

#include "catalog/catalog.hpp"
#include "execution/query_result.hpp"
#include "expression/expression.hpp"
#include "planner/abstract_plan_node.hpp"
#include "planner/query_planner.hpp"

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
    Result<QueryResult> executeQuery(const std::string& sql);

    size_t getLastExecutionRowCount() const { return last_execution_row_count_; }
    double getLastExecutionTimeMs() const { return last_execution_time_ms_; }

private:
    Result<QueryResult> executePlan(std::unique_ptr<AbstractPlanNode> plan);

    Catalog& catalog_;
    QueryPlanner planner_;
    ExecutionContext context_;

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
