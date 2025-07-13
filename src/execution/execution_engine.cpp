#include "execution/execution_engine.hpp"
#include "SQLParser.h"
#include <sstream>
#include <stdexcept>

namespace velodb {

// TODO: Implement full execution engine with late materialization

// QueryResult implementation
QueryResult::QueryResult(std::unique_ptr<Schema> schema)
    : schema_(std::move(schema))
{
}

void QueryResult::addTuple(const Tuple& tuple)
{
    tuples_.push_back(tuple);
}

void QueryResult::addTuple(Tuple&& tuple)
{
    tuples_.push_back(std::move(tuple));
}

std::string QueryResult::toString() const
{
    std::stringstream ss;
    ss << "QueryResult: " << tuples_.size() << " rows\n";
    ss << "Schema: " << schema_->toString() << "\n";

    for (const auto& tuple : tuples_) {
        ss << tuple.toString() << "\n";
    }

    return ss.str();
}

// LateMaterializationOptimizer implementation
LateMaterializationOptimizer::MaterializationPlan
LateMaterializationOptimizer::analyzePlan(const AbstractPlanNode& plan_node) const
{
    // TODO: Implement sophisticated late materialization analysis
    MaterializationPlan plan;

    std::vector<size_t> const required_columns = getRequiredColumns(plan_node);

    // TODO: Determine which columns to materialize early vs late
    // For now, materialize all columns late
    plan.late_columns_ = required_columns;

    return plan;
}

std::vector<size_t> LateMaterializationOptimizer::getRequiredColumns(const AbstractPlanNode& plan_node) const
{
    // TODO: Implement column requirement analysis
    std::vector<size_t> required_columns;
    analyzeNode(plan_node, required_columns);
    return required_columns;
}

void LateMaterializationOptimizer::analyzeNode(const AbstractPlanNode& node,
    std::vector<size_t>& required_columns)
{
    // TODO: Implement recursive analysis of plan nodes
    // For now, assume all columns are required
    for (size_t i = 0; i < node.getOutputSchema().getColumnCount(); ++i) {
        required_columns.push_back(i);
    }
}

// ExecutionEngine implementation
ExecutionEngine::ExecutionEngine(Catalog* catalog)
    : catalog_(catalog)
{
    planner_ = std::make_unique<QueryPlanner>(catalog);
    context_ = std::make_unique<ExecutionContext>(catalog);
    optimizer_ = std::make_unique<LateMaterializationOptimizer>();
}

std::unique_ptr<QueryResult> ExecutionEngine::executeQuery(const std::string& sql)
{
    // TODO: Implement full SQL query execution
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);

    if (!result.isValid()) {
        throw std::runtime_error("Invalid SQL query: " + std::string(result.errorMsg()));
    }

    if (result.size() != 1) {
        throw std::runtime_error("Multiple statements not supported");
    }

    return executeStatement(result.getStatement(0));
}

std::unique_ptr<QueryResult> ExecutionEngine::executeStatement(const hsql::SQLStatement* statement)
{
    // TODO: Implement statement type dispatch
    switch (statement->type()) {
    case hsql::kStmtSelect:
        return executeSelect(dynamic_cast<const hsql::SelectStatement*>(statement));
    default:
        throw std::runtime_error("Statement type not supported");
    }
}

std::unique_ptr<QueryResult> ExecutionEngine::executeSelect(const hsql::SelectStatement* select_stmt)
{
    // TODO: Implement SELECT statement execution
    auto plan = planner_->planSelect(select_stmt);
    return executePlan(std::move(plan));
}

std::unique_ptr<QueryResult> ExecutionEngine::executePlan(std::unique_ptr<AbstractPlanNode> plan)
{
    // TODO: Implement plan execution with late materialization optimization
    auto op = createOperatorTree(*plan);
    auto mat_plan = optimizer_->analyzePlan(*plan);
    return executeWithLateMaterialization(std::move(op), mat_plan);
}

// std::unique_ptr<QueryResult> ExecutionEngine::ExecuteOperator(std::unique_ptr<AbstractOperator> op)
// {
//     // TODO: Implement operator execution
//     auto result = std::make_unique<QueryResult>(
//         op->getOutputSchema().clone());

//     collectResults(op.get(), result.get());
//     return result;
// }

std::unique_ptr<QueryResult> ExecutionEngine::executeWithLateMaterialization(
    std::unique_ptr<AbstractOperator> op,
    const LateMaterializationOptimizer::MaterializationPlan& mat_plan)
{
    // TODO: Implement late materialization execution
    auto result = std::make_unique<QueryResult>(
        op->getOutputSchema().clone());

    collectResultsWithLateMaterialization(op.get(), result.get(), mat_plan);
    return result;
}

std::unique_ptr<AbstractOperator> ExecutionEngine::createOperatorTree(const AbstractPlanNode& plan_node)
{
    // TODO: Implement plan node to operator conversion
    return plan_node.createOperator(context_.get());
}

void ExecutionEngine::collectResults(AbstractOperator* op, QueryResult* result)
{
    // TODO: Implement result collection
    op->init();

    Tuple tuple(op->getOutputSchema());
    RowId row_id = 0;

    while (op->next(&tuple, &row_id)) {
        result->addTuple(tuple);
    }

    last_execution_row_count_ = result->getRowCount();
}

void ExecutionEngine::collectResultsWithLateMaterialization(
    AbstractOperator* op,
    QueryResult* result,
    [[maybe_unused]] const LateMaterializationOptimizer::MaterializationPlan& mat_plan)
{
    // TODO: Implement late materialization result collection
    // For now, fall back to regular collection
    collectResults(op, result);
}

// ExecutionStats implementation
std::string ExecutionStats::toString() const
{
    std::stringstream ss;
    ss << "Execution Stats:\n";
    ss << "  Rows processed: " << rows_processed_ << "\n";
    ss << "  Execution time: " << execution_time_ms_ << " ms\n";
    return ss.str();
}

} // namespace velodb
