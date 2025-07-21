#pragma once

#include "catalog/catalog.hpp"
#include "expression.hpp"
#include "operator.hpp"
#include "planner.hpp"
#include <memory>
#include <vector>

// Forward declarations for SQL parser
namespace hsql {
struct SQLStatement;
struct SelectStatement;
}

namespace velodb {

// Query execution result
class QueryResult {
public:
    explicit QueryResult(std::unique_ptr<Schema> schema);
    ~QueryResult() = default;

    // Move constructor and assignment
    QueryResult(QueryResult&& other) noexcept = default;
    QueryResult& operator=(QueryResult&& other) noexcept = default;

    // Delete copy constructor and assignment
    QueryResult(const QueryResult&) = delete;
    QueryResult& operator=(const QueryResult&) = delete;

    void addTuple(const Tuple& tuple);
    void addTuple(Tuple&& tuple);

    [[nodiscard]] const Schema& getSchema() const { return *schema_; }
    [[nodiscard]] const std::vector<Tuple>& getTuples() const { return tuples_; }
    [[nodiscard]] size_t getRowCount() const { return tuples_.size(); }
    [[nodiscard]] bool isEmpty() const { return tuples_.empty(); }

    [[nodiscard]] std::string toString() const;

private:
    std::unique_ptr<Schema> schema_;
    std::vector<Tuple> tuples_;
};

// Late materialization optimizer
class LateMaterializationOptimizer {
public:
    LateMaterializationOptimizer() = default;
    ~LateMaterializationOptimizer() = default;

    // Analyze query plan and determine optimal materialization strategy
    struct MaterializationPlan {
        std::vector<size_t> late_columns_; // Columns to materialize late
        bool use_late_materialization_ {}; // Whether to use late materialization
    };

    [[nodiscard]] MaterializationPlan analyzePlan(const AbstractPlanNode& plan_node) const;

    // Determine which columns are needed at each operator
    [[nodiscard]] std::vector<size_t> getRequiredColumns(const AbstractPlanNode& plan_node) const;

private:
    static void analyzeNode(const AbstractPlanNode& node,
        std::vector<size_t>& required_columns);
};

// Main execution engine
class ExecutionEngine {
public:
    explicit ExecutionEngine(Catalog* catalog);
    ~ExecutionEngine() = default;

    // Delete copy constructor and assignment
    ExecutionEngine(const ExecutionEngine&) = delete;
    ExecutionEngine& operator=(const ExecutionEngine&) = delete;

    // Main execution interface
    std::unique_ptr<QueryResult> executeQuery(const std::string& sql);
    std::unique_ptr<QueryResult> executeStatement(const hsql::SQLStatement* statement);
    std::unique_ptr<QueryResult> executeSelect(const hsql::SelectStatement* select_stmt);

    // Plan execution
    std::unique_ptr<QueryResult> executePlan(std::unique_ptr<AbstractPlanNode> plan);

    // Late materialization execution
    std::unique_ptr<QueryResult> executeWithLateMaterialization(
        std::unique_ptr<AbstractOperator> op,
        const LateMaterializationOptimizer::MaterializationPlan& mat_plan);

    [[nodiscard]] size_t getLastExecutionRowCount() const { return last_execution_row_count_; }
    [[nodiscard]] double getLastExecutionTimeMs() const { return last_execution_time_ms_; }

private:
    // Helper methods
    std::unique_ptr<AbstractOperator> createOperatorTree(const AbstractPlanNode& plan_node);
    
    // New execution methods for row ID pipeline
    std::unique_ptr<QueryResult> executeOperatorTree(std::unique_ptr<AbstractOperator> root_op);
    std::unique_ptr<QueryResult> executeWithRowIdCollection(std::unique_ptr<AbstractOperator> op);
    
    // Legacy methods
    void collectResults(AbstractOperator* op, QueryResult* result);
    void collectResultsWithLateMaterialization(
        AbstractOperator* op,
        QueryResult* result,
        const LateMaterializationOptimizer::MaterializationPlan& mat_plan);

    Catalog* catalog_;
    std::unique_ptr<QueryPlanner> planner_;
    std::unique_ptr<ExecutionContext> context_;
    std::unique_ptr<LateMaterializationOptimizer> optimizer_;

    size_t last_execution_row_count_ { 0 };
    double last_execution_time_ms_ { 0.0 };
};

// Utility class for query execution statistics
class ExecutionStats {
public:
    ExecutionStats() { }

    void setRowsProcessed(size_t rows) { rows_processed_ = rows; }
    void setExecutionTime(double time_ms) { execution_time_ms_ = time_ms; }

    [[nodiscard]] size_t getRowsProcessed() const { return rows_processed_; }
    [[nodiscard]] double getExecutionTime() const { return execution_time_ms_; }

    [[nodiscard]] std::string toString() const;

private:
    size_t rows_processed_ { 0 };
    double execution_time_ms_ { 0.0 };
};

} // namespace velodb
