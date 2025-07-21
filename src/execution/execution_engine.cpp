#include "execution/execution_engine.hpp"
#include "execution/operator/projection_operator.hpp"
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
    // Create the operator tree from the plan
    auto op = createOperatorTree(*plan);
    
    // Execute the operator tree 
    // The ProjectionOperator at the root will handle materialization
    return executeOperatorTree(std::move(op));
}

std::unique_ptr<QueryResult> ExecutionEngine::executeOperatorTree(std::unique_ptr<AbstractOperator> root_op)
{
    // NEW PROPER EXECUTION PIPELINE:
    // The execute() method on any operator will traverse the tree and execute all children first
    // This implements bottom-up execution with proper tree traversal
    
    return root_op->execute();
}

std::unique_ptr<QueryResult> ExecutionEngine::executeWithRowIdCollection(std::unique_ptr<AbstractOperator> op)
{
    // Simple execution: collect row IDs then materialize from the first table in catalog
    // This is a simplified approach - in practice, the planner should set up proper materialization
    
    auto result = std::make_unique<QueryResult>(op->getOutputSchema().clone());
    
    // Initialize and collect row IDs
    op->init();
    std::vector<RowId> row_ids;
    RowId row_id;
    while (op->nextRowId(&row_id)) {
        row_ids.push_back(row_id);
    }
    
    // For now, just return empty result with collected row count
    // TODO: Implement proper materialization logic
    last_execution_row_count_ = row_ids.size();
    
    return result;
}

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
    // Use late materialization interface
    op->init();

    // Collect all row IDs first
    std::vector<RowId> row_ids;
    RowId row_id;
    while (op->nextRowId(&row_id)) {
        row_ids.push_back(row_id);
    }

    // Materialize all columns for all rows
    if (!row_ids.empty()) {
        std::vector<size_t> all_columns;
        for (size_t i = 0; i < op->getOutputSchema().getColumnCount(); ++i) {
            all_columns.push_back(i);
        }

        std::vector<Tuple> tuples;

        for (auto& tuple : tuples) {
            result->addTuple(std::move(tuple));
        }
    }

    last_execution_row_count_ = result->getRowCount();
}

void ExecutionEngine::collectResultsWithLateMaterialization(
    AbstractOperator* op,
    QueryResult* result,
    [[maybe_unused]] const LateMaterializationOptimizer::MaterializationPlan& mat_plan)
{
    // Implement late materialization result collection
    op->init();

    // Collect all row IDs first
    std::vector<RowId> row_ids;
    RowId row_id;
    while (op->nextRowId(&row_id)) {
        row_ids.push_back(row_id);
    }

    // Materialize only the required columns
    if (!row_ids.empty()) {
        std::vector<Tuple> tuples;

        for (auto& tuple : tuples) {
            result->addTuple(std::move(tuple));
        }
    }

    last_execution_row_count_ = result->getRowCount();
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
