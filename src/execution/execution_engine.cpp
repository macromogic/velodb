#include "execution/execution_engine.hpp"

#include "common/exception.hpp"
#include "common/profiler.hpp"
#include "operator/projection_operator.hpp"

#include <SQLParser.h>
#include <fmt/core.h>

#include <stdexcept>

namespace velodb {

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
    PROFILE_SCOPE("Execute Query (full)");
    hsql::SQLParserResult sql_result;
    {
        PROFILE_SCOPE("SQL Parsing");
        hsql::SQLParser::parse(sql, &sql_result);
    }

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
    auto query_result = executePlan(std::move(plan));
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

    std::unique_ptr<AbstractOperator> operator_tree;
    {
        PROFILE_SCOPE("Create Operator Tree");
        operator_tree = plan->createOperator(context_);
    }
    if (!operator_tree) {
        return Result<QueryResult>::failure("Failed to create operator tree from plan");
    }

    QueryResult result(operator_tree->getOutputSchema().clone());
    {
        PROFILE_SCOPE("Execute Operator Tree");
        while (true) {
            auto batch_result = operator_tree->next();
            if (!batch_result) {
                return Result<QueryResult>::failure(batch_result.error());
            }
            auto batch = std::move(batch_result.value());
            if (batch.getRowCount() == 0) {
                break; // No more results
            }
            {
                PROFILE_SCOPE("Append Batch to Result");
                result.append(std::move(batch));
            }
        }
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
