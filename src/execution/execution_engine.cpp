#include "execution/execution_engine.hpp"
#include "operator/projection_operator.hpp"
#include "common/exception.hpp"
#include "SQLParser.h"
#include <sstream>
#include <stdexcept>

namespace velodb {

// TODO: Implement full execution engine with late materialization

// QueryResult implementation - Column-based storage
QueryResult::QueryResult(std::unique_ptr<Schema> schema)
    : schema_(std::move(schema))
    , row_count_(0)
{
    initializeColumns();
}

void QueryResult::initializeColumns()
{
    const size_t column_count = schema_->getColumnCount();
    columns_.resize(column_count);
    // Each column vector starts empty and will grow as rows are inserted
}

void QueryResult::ensureColumnCapacity(size_t new_row_count)
{
    for (auto& column : columns_) {
        if (column.capacity() < new_row_count) {
            column.reserve(new_row_count);
        }
    }
}

void QueryResult::insertRowInternal(const std::vector<Value>& values)
{
    if (values.size() != columns_.size()) {
        VELODB_THROW(ExecutionError, "Row value count does not match schema column count");
    }
    
    ensureColumnCapacity(row_count_ + 1);
    
    // Insert each value into its corresponding column
    for (size_t col_idx = 0; col_idx < values.size(); ++col_idx) {
        columns_[col_idx].push_back(values[col_idx]);
    }
    
    ++row_count_;
}

// Column-based insertion methods
void QueryResult::addRow(const std::vector<Value>& values)
{
    insertRowInternal(values);
}

void QueryResult::addRow(std::vector<Value>&& values)
{
    if (values.size() != columns_.size()) {
        VELODB_THROW(ExecutionError, "Row value count does not match schema column count");
    }
    
    ensureColumnCapacity(row_count_ + 1);
    
    // Move each value into its corresponding column
    for (size_t col_idx = 0; col_idx < values.size(); ++col_idx) {
        columns_[col_idx].push_back(std::move(values[col_idx]));
    }
    
    ++row_count_;
}

void QueryResult::addBatchRows(const std::vector<std::vector<Value>>& rows)
{
    if (rows.empty()) return;
    
    ensureColumnCapacity(row_count_ + rows.size());
    
    for (const auto& row : rows) {
        insertRowInternal(row);
    }
}

// Column-based access methods
Value QueryResult::getValue(RowId row_id, size_t column_index) const
{
    if (row_id >= row_count_) {
        VELODB_THROW(ExecutionError, "Row ID out of range");
    }
    if (column_index >= columns_.size()) {
        VELODB_THROW(ExecutionError, "Column index out of range");
    }
    
    return columns_[column_index][row_id];
}

std::vector<Value> QueryResult::getValues(RowId row_id, const std::vector<size_t>& column_indices) const
{
    if (row_id >= row_count_) {
        VELODB_THROW(ExecutionError, "Row ID out of range");
    }
    
    std::vector<Value> values;
    values.reserve(column_indices.size());
    
    for (size_t const col_idx : column_indices) {
        if (col_idx >= columns_.size()) {
            VELODB_THROW(ExecutionError, "Column index out of range");
        }
        values.push_back(columns_[col_idx][row_id]);
    }
    return values;
}

const ValueVector& QueryResult::getColumn(size_t column_index) const
{
    if (column_index >= columns_.size()) {
        VELODB_THROW(ExecutionError, "Column index out of range");
    }
    return columns_[column_index];
}

std::vector<Value> QueryResult::getColumnValues(size_t column_index, const std::vector<RowId>& row_ids) const
{
    if (column_index >= columns_.size()) {
        VELODB_THROW(ExecutionError, "Column index out of range");
    }
    
    std::vector<Value> values;
    values.reserve(row_ids.size());
    
    for (RowId const row_id : row_ids) {
        if (row_id >= row_count_) {
            VELODB_THROW(ExecutionError, "Row ID out of range");
        }
        values.push_back(columns_[column_index][row_id]);
    }
    
    return values;
}

std::vector<ValueVector> QueryResult::getColumns(const std::vector<size_t>& column_indices) const
{
    std::vector<ValueVector> result;
    result.reserve(column_indices.size());
    
    for (size_t const col_idx : column_indices) {
        if (col_idx >= columns_.size()) {
            VELODB_THROW(ExecutionError, "Column index out of range");
        }
        result.push_back(columns_[col_idx]);
    }
    
    return result;
}

std::vector<RowId> QueryResult::getAllRowIds() const
{
    std::vector<RowId> row_ids;
    row_ids.reserve(row_count_);
    
    for (RowId row_id = 0; row_id < row_count_; ++row_id) {
        row_ids.push_back(row_id);
    }
    
    return row_ids;
}

std::string QueryResult::toString() const
{
    std::stringstream ss;
    ss << "QueryResult: " << row_count_ << " rows\n";
    ss << "Schema: " << schema_->toString() << "\n";

    return ss.str();
}

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
    std::stringstream ss;
    ss << "Execution Stats:\n";
    ss << "  Rows processed: " << rows_processed_ << "\n";
    ss << "  Execution time: " << execution_time_ms_ << " ms\n";
    return ss.str();
}

} // namespace velodb
