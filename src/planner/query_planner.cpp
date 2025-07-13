#include "planner/query_planner.hpp"
#include "planner/seq_scan_plan_node.hpp"
#include "execution/constant_expression.hpp"
#include "execution/column_ref_expression.hpp"
#include "execution/comparison_expression.hpp"
#include "types/data_type.hpp"
#include "catalog/catalog.hpp"
#include "SQLParser.h"
#include <stdexcept>

namespace velodb {

// QueryPlanner implementation
QueryPlanner::QueryPlanner(Catalog* catalog)
    : catalog_(catalog)
{
}

std::unique_ptr<AbstractPlanNode> QueryPlanner::planQuery(const hsql::SQLStatement* statement)
{
    // TODO: Implement full query planning
    switch (statement->type()) {
    case hsql::kStmtSelect:
        return planSelect(dynamic_cast<const hsql::SelectStatement*>(statement));
    default:
        throw std::runtime_error("Statement type not supported in planner");
    }
}

std::unique_ptr<AbstractPlanNode> QueryPlanner::planSelect(const hsql::SelectStatement* select_stmt)
{
    // TODO: Implement comprehensive SELECT planning

    // Plan the FROM clause
    if (select_stmt->fromTable == nullptr) {
        throw std::runtime_error("SELECT without FROM not supported");
    }

    auto scan_plan = planTableRef(select_stmt->fromTable);

    // TODO: Plan WHERE clause
    if (select_stmt->whereClause != nullptr) {
        auto predicate = planExpression(select_stmt->whereClause);
        // For now, push predicate down to scan
        // TODO: Create separate filter node when needed
    }

    // TODO: Plan SELECT list (projection)
    // TODO: Plan ORDER BY
    // TODO: Plan LIMIT

    return scan_plan;
}

std::unique_ptr<AbstractPlanNode> QueryPlanner::planTableRef(const hsql::TableRef* table_ref)
{
    // TODO: Implement full table reference planning
    switch (table_ref->type) {
    case hsql::kTableName: {
        std::string const table_name = table_ref->name;
        TableBase* table = catalog_->getTable(table_name);
        if (table == nullptr) {
            throw std::runtime_error("Table not found: " + table_name);
        }
        return std::make_unique<SeqScanPlanNode>(*table);
    }
    case hsql::kTableSelect:
        // TODO: Handle subqueries
        throw std::runtime_error("Subqueries not implemented");
    case hsql::kTableJoin:
        // TODO: Handle joins
        throw std::runtime_error("Joins not implemented");
    default:
        throw std::runtime_error("Unsupported table reference type");
    }
}

std::unique_ptr<AbstractExpression> QueryPlanner::planExpression(const hsql::Expr* expr)
{
    // TODO: Implement comprehensive expression planning
    switch (expr->type) {
    case hsql::kExprLiteralInt:
        return std::make_unique<ConstantExpression>(Value::createInteger(expr->ival));
    case hsql::kExprLiteralFloat:
        return std::make_unique<ConstantExpression>(Value::createDouble(expr->fval));
    case hsql::kExprLiteralString:
        return std::make_unique<ConstantExpression>(Value::createString(expr->name));
    case hsql::kExprColumnRef:
        return planColumnRef(expr);
    case hsql::kExprOperator:
        return planOperator(expr);
    default:
        throw std::runtime_error("Expression type not implemented");
    }
}

std::unique_ptr<Schema> QueryPlanner::inferSelectSchema([[maybe_unused]] const hsql::SelectStatement* select_stmt,
    const Schema& input_schema)
{
    // TODO: Implement schema inference from SELECT list
    // For now, return a clone of the input schema
    return input_schema.clone();
}

std::unique_ptr<AbstractExpression> QueryPlanner::planColumnRef(const hsql::Expr* expr)
{
    // TODO: Implement column reference planning with proper type inference
    std::string const column_name = expr->name;
    auto type = std::make_unique<IntegerType>(); // TODO: Infer actual type
    return std::make_unique<ColumnRefExpression>(column_name, std::move(type));
}

std::unique_ptr<AbstractExpression> QueryPlanner::planLiteral(const hsql::Expr* expr)
{
    // TODO: Implement literal planning
    switch (expr->type) {
    case hsql::kExprLiteralInt:
        return std::make_unique<ConstantExpression>(Value::createInteger(expr->ival));
    case hsql::kExprLiteralFloat:
        return std::make_unique<ConstantExpression>(Value::createDouble(expr->fval));
    case hsql::kExprLiteralString:
        return std::make_unique<ConstantExpression>(Value::createString(expr->name));
    default:
        throw std::runtime_error("Unsupported literal type");
    }
}

std::unique_ptr<AbstractExpression> QueryPlanner::planOperator(const hsql::Expr* expr)
{
    // TODO: Implement operator planning
    switch (expr->opType) {
    case hsql::kOpEquals: {
        auto left = planExpression(expr->expr);
        auto right = planExpression(expr->expr2);
        return std::make_unique<ComparisonExpression>(
            ComparisonType::EQUAL, std::move(left), std::move(right));
    }
    // TODO: Implement other operators
    default:
        throw std::runtime_error("Operator not implemented");
    }
}

} // namespace velodb
