#include "execution/execution_engine.hpp"

#include "common/exception.hpp"
#include "operator/projection_operator.hpp"

#include <SQLParser.h>
#include <fmt/core.h>

#include <stdexcept>

namespace velodb {

// TODO: Implement full execution engine with late materialization

// ExecutionEngine implementation
ExecutionEngine::ExecutionEngine(Catalog& catalog)
    : catalog_(catalog)
{
    planner_ = std::make_unique<QueryPlanner>(catalog);
    context_ = std::make_unique<ExecutionContext>(catalog);
}

Result<View> ExecutionEngine::executeQuery(const std::string& sql)
{
    // TODO: Implement full SQL query execution
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);

    if (!result.isValid()) {
        return Result<View>::failure("SQL parsing error: " + std::string(result.errorMsg()));
    }

    if (result.size() != 1) {
        return Result<View>::failure("Multiple statements not supported");
    }

    return executeStatement(result.getStatement(0));
}

Result<View> ExecutionEngine::executeStatement(const hsql::SQLStatement* statement)
{
    // TODO: Implement statement type dispatch
    switch (statement->type()) {
    case hsql::kStmtSelect:
        return executeSelect(dynamic_cast<const hsql::SelectStatement*>(statement));
    default:
        return Result<View>::failure("Non-select statements not supported");
    }
}

Result<View> ExecutionEngine::executeSelect(const hsql::SelectStatement* select_stmt)
{
    // TODO: Implement SELECT statement execution
    auto plan = planner_->planSelect(select_stmt);
    return executePlan(std::move(plan));
}

Result<View> ExecutionEngine::executePlan(std::unique_ptr<AbstractPlanNode> plan)
{
    // Create the operator tree from the plan
    auto op = createOperatorTree(*plan);

    // Execute the operator tree
    return op->execute();
}

std::unique_ptr<AbstractOperator> ExecutionEngine::createOperatorTree(const AbstractPlanNode& plan_node)
{
    // TODO: Implement plan node to operator conversion
    return plan_node.createOperator(*context_);
}

// ExecutionStats implementation
std::string ExecutionStats::toString() const
{
    return fmt::format("Execution Stats:\n  Rows processed: {}\n  Execution time: {} ms\n",
                       rows_processed_,
                       execution_time_ms_);
}

} // namespace velodb
