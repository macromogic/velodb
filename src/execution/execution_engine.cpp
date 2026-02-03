#include "execution/execution_engine.hpp"

#include "common/exception.hpp"
#include "common/profiler.hpp"
#include "operator/projection_operator.hpp"
#include "planner/seq_scan_plan_node.hpp"

#include <SQLParser.h>
#include <fmt/core.h>

namespace velodb {

// Helper to collect table names from plan tree
static void collectAccessedTables(const AbstractPlanNode* node, std::vector<std::string>& tables)
{
    if (!node) {
        return;
    }

    if (node->getPlanType() == PlanType::SEQ_SCAN) {
        const auto* scan = static_cast<const SeqScanPlanNode*>(node);
        tables.push_back(scan->getTable().getName());
    }

    for (const auto& child : node->getChildren()) {
        collectAccessedTables(child.get(), tables);
    }
}

// ExecutionEngine implementation
ExecutionEngine::ExecutionEngine(Catalog& catalog, TaskManager& task_manager)
    : catalog_(catalog)
    , task_manager_(task_manager)
    , planner_(catalog)
    , context_(catalog, task_manager)
{
}

ExecutionEngine::ExecutionEngine(ExecutionEngine&& other) noexcept
    : catalog_(other.catalog_)
    , task_manager_(other.task_manager_)
    , planner_(std::move(other.planner_))
    , context_(std::move(other.context_))
    , last_execution_row_count_(other.last_execution_row_count_)
    , last_execution_time_ms_(other.last_execution_time_ms_)
{
    other.last_execution_row_count_ = 0;
    other.last_execution_time_ms_ = 0.0;
}

ExecutionEngine& ExecutionEngine::operator=(ExecutionEngine&& other) noexcept
{
    if (this != &other) {
        catalog_ = std::move(other.catalog_);
        task_manager_ = std::move(other.task_manager_);
        planner_ = std::move(other.planner_);
        context_ = std::move(other.context_);
        last_execution_row_count_ = other.last_execution_row_count_;
        last_execution_time_ms_ = other.last_execution_time_ms_;

        other.last_execution_row_count_ = 0;
        other.last_execution_time_ms_ = 0.0;
    }
    return *this;
}

Result<QueryResult> ExecutionEngine::executeQuery(const std::string& sql, QueryStatistics* stats)
{
    hsql::SQLParserResult sql_result;
    hsql::SQLParser::parse(sql, &sql_result);

    if (!sql_result.isValid()) {
        return Result<QueryResult>::failure("SQL parsing error: " + std::string(sql_result.errorMsg()));
    }
    if (sql_result.size() != 1) {
        return Result<QueryResult>::failure("Multiple statements not supported");
    }

    auto* statement = sql_result.getStatement(0);
    if (statement->type() != hsql::kStmtSelect) {
        return Result<QueryResult>::failure("Non-select statements not supported");
    }
    auto t0 = std::chrono::high_resolution_clock::now();
    auto plan = planner_.planSelect(static_cast<const hsql::SelectStatement*>(statement));
    auto t1 = std::chrono::high_resolution_clock::now();

    // Collect accessed tables and notify oblivious manager
    std::vector<std::string> accessed_tables;
    collectAccessedTables(plan.get(), accessed_tables);

    // Begin oblivious query (wait for pending shuffles)
    if (catalog_.hasObliviousManager()) {
        catalog_.getObliviousManager().beginQuery(accessed_tables);
        // Mark all tables as accessed
        for (const auto& table_name : accessed_tables) {
            catalog_.getObliviousManager().markAccessed(table_name);
        }
    }

    auto query_result = executePlan(std::move(plan));

    // End oblivious query (trigger async shuffles)
    if (catalog_.hasObliviousManager()) {
        catalog_.getObliviousManager().endQuery();
    }

    auto t2 = std::chrono::high_resolution_clock::now();
    if (stats) {
        stats->planning_time = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0);
        stats->execution_time = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1);
        if (query_result) {
            stats->rows_processed = query_result.value().getRowCount();
        }
    }
    return query_result;
}

Result<QueryResult> ExecutionEngine::executePlan(std::unique_ptr<AbstractPlanNode> plan)
{
    if (!plan) {
        return Result<QueryResult>::failure("Cannot execute null plan");
    }

    auto operator_tree = plan->createOperator(context_);
    if (!operator_tree) {
        return Result<QueryResult>::failure("Failed to create operator tree from plan");
    }

    QueryResult result(operator_tree->getOutputSchema().clone());
    while (true) {
        auto batch_result = operator_tree->next();
        if (!batch_result) {
            return Result<QueryResult>::failure(batch_result.error());
        }
        auto batch = std::move(batch_result.value());
        if (batch.getRowCount() == 0) {
            break; // No more results
        }
        result.append(std::move(batch));
    }
    return Result<QueryResult>::success(std::move(result));
}

// ExecutionStats implementation
std::string ExecutionStats::toString() const
{
    return fmt::format("Execution Stats:\n  Rows processed: {}\n  Execution time: {} ms\n",
                       rows_processed_,
                       execution_time_ms_);
}

} // namespace velodb
