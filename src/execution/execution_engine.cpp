#include "execution/execution_engine.hpp"

#include "common/exception.hpp"
#include "operator/projection_operator.hpp"

#include <SQLParser.h>
#include <fmt/core.h>

#include <stdexcept>

namespace velodb {

// ExecutionEngine implementation
ExecutionEngine::ExecutionEngine(Catalog& catalog)
    : catalog_(catalog)
{
    planner_ = std::make_unique<QueryPlanner>(catalog);
    context_ = std::make_unique<ExecutionContext>(catalog);
}

Result<QueryResult> ExecutionEngine::executeQuery(const std::string& sql)
{
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);

    if (!result.isValid()) {
        return Result<QueryResult>::failure("SQL parsing error: " + std::string(result.errorMsg()));
    }
    if (result.size() != 1) {
        return Result<QueryResult>::failure("Multiple statements not supported");
    }

    auto* statement = result.getStatement(0);
    if (statement->type() != hsql::kStmtSelect) {
        return Result<QueryResult>::failure("Non-select statements not supported");
    }
    auto plan = planner_->planSelect(static_cast<const hsql::SelectStatement*>(statement));
    return executePlan(std::move(plan));
}

Result<QueryResult> ExecutionEngine::executePlan(std::unique_ptr<AbstractPlanNode> plan)
{
    if (!plan) {
        return Result<QueryResult>::failure("Cannot execute null plan");
    }

    auto operator_tree = plan->createOperator(*context_);
    if (!operator_tree) {
        return Result<QueryResult>::failure("Failed to create operator tree from plan");
    }

    QueryResult result(operator_tree->getOutputSchema().cloneUnique());
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
