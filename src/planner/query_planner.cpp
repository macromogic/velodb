#include "planner/query_planner.hpp"
#include "SQLParser.h"
#include "catalog/catalog.hpp"
#include "catalog/column.hpp"
#include "catalog/schema.hpp"
#include "common/exception.hpp"
#include "expression/expression.hpp"
#include "planner/projection_plan_node.hpp"
#include "planner/scan_filter_plan_node.hpp"
#include "types/data_type.hpp"

namespace velodb {

// QueryPlanner implementation
QueryPlanner::QueryPlanner(Catalog& catalog)
    : catalog_(catalog)
{
}

std::unique_ptr<AbstractPlanNode> QueryPlanner::planSelect(const hsql::SelectStatement* select_stmt)
{
    // Plan the FROM clause
    if (select_stmt->fromTable == nullptr) {
        VELODB_THROW(ExecutionError, "SELECT without FROM not supported");
    }

    // Plan WHERE clause and merge with scan
    std::unique_ptr<AbstractExpression> predicate = nullptr;
    if (select_stmt->whereClause != nullptr) {
        predicate = planExpression(select_stmt->whereClause);
    }

    auto plan = planTableRef(select_stmt->fromTable, std::move(predicate));

    // Plan SELECT list (projection)
    if (select_stmt->selectList && !select_stmt->selectList->empty()) {
        auto projection_expressions = planSelectList(select_stmt->selectList);
        auto& input_schema = catalog_.getTable(select_stmt->fromTable->name)->getSchema();
        auto projection_schema = inferProjectionSchema(projection_expressions, input_schema);
        auto projection_plan = std::make_unique<ProjectionPlanNode>(
            std::move(projection_schema), std::move(projection_expressions));
        projection_plan->addChild(std::move(plan));
        plan = std::move(projection_plan);
    }

    // TODO: Plan ORDER BY
    // TODO: Plan LIMIT

    return plan;
}

std::unique_ptr<AbstractPlanNode> QueryPlanner::planTableRef(const hsql::TableRef* table_ref, std::unique_ptr<AbstractExpression> predicate)
{
    // TODO: Implement full table reference planning
    switch (table_ref->type) {
    case hsql::kTableName: {
        std::string const table_name = table_ref->name;
        TableBase* table = catalog_.getTable(table_name);
        if (table == nullptr) {
            VELODB_THROW(CatalogError, "Table not found: " + table_name);
        }
        return std::make_unique<ScanFilterPlanNode>(*table, inferScanFilterSchema(table->getSchema()), std::move(predicate));
    }
    case hsql::kTableSelect:
        // TODO: Handle subqueries
        VELODB_THROW(ExecutionError, "Subqueries not implemented");
    case hsql::kTableJoin:
        // TODO: Handle joins
        VELODB_THROW(ExecutionError, "Joins not implemented");
    default:
        VELODB_THROW(ExecutionError, "Unsupported table reference type");
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
    case hsql::kExprStar:
        VELODB_THROW(ExecutionError, "* expression should be handled in planSelectList, not planExpression");
    default:
        VELODB_THROW(ExecutionError, "Expression type not implemented");
    }
}

// UNUSED
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
        VELODB_THROW(ExecutionError, "Unsupported literal type");
    }
}

std::unique_ptr<AbstractExpression> QueryPlanner::planOperator(const hsql::Expr* expr)
{
    // Implement operator planning for all comparison operators
    switch (expr->opType) {
    case hsql::kOpEquals: {
        auto left = planExpression(expr->expr);
        auto right = planExpression(expr->expr2);
        return std::make_unique<ComparisonExpression>(
            ComparisonType::EQUAL, std::move(left), std::move(right));
    }
    case hsql::kOpNotEquals: {
        auto left = planExpression(expr->expr);
        auto right = planExpression(expr->expr2);
        return std::make_unique<ComparisonExpression>(
            ComparisonType::NOT_EQUAL, std::move(left), std::move(right));
    }
    case hsql::kOpLess: {
        auto left = planExpression(expr->expr);
        auto right = planExpression(expr->expr2);
        return std::make_unique<ComparisonExpression>(
            ComparisonType::LESS_THAN, std::move(left), std::move(right));
    }
    case hsql::kOpLessEq: {
        auto left = planExpression(expr->expr);
        auto right = planExpression(expr->expr2);
        return std::make_unique<ComparisonExpression>(
            ComparisonType::LESS_THAN_OR_EQUAL, std::move(left), std::move(right));
    }
    case hsql::kOpGreater: {
        auto left = planExpression(expr->expr);
        auto right = planExpression(expr->expr2);
        return std::make_unique<ComparisonExpression>(
            ComparisonType::GREATER_THAN, std::move(left), std::move(right));
    }
    case hsql::kOpGreaterEq: {
        auto left = planExpression(expr->expr);
        auto right = planExpression(expr->expr2);
        return std::make_unique<ComparisonExpression>(
            ComparisonType::GREATER_THAN_OR_EQUAL, std::move(left), std::move(right));
    }
    case hsql::kOpLike: {
        auto left = planExpression(expr->expr);
        auto right = planExpression(expr->expr2);
        return std::make_unique<ComparisonExpression>(
            ComparisonType::LIKE, std::move(left), std::move(right));
    }
    case hsql::kOpNotLike: {
        auto left = planExpression(expr->expr);
        auto right = planExpression(expr->expr2);
        return std::make_unique<ComparisonExpression>(
            ComparisonType::NOT_LIKE, std::move(left), std::move(right));
    }
    case hsql::kOpAnd: {
        auto left = planExpression(expr->expr);
        auto right = planExpression(expr->expr2);
        std::vector<std::unique_ptr<AbstractExpression>> children;
        children.push_back(std::move(left));
        children.push_back(std::move(right));
        return std::make_unique<ConjunctionExpression>(
            ConjunctionType::AND, std::move(children));
    }
    case hsql::kOpOr: {
        auto left = planExpression(expr->expr);
        auto right = planExpression(expr->expr2);
        std::vector<std::unique_ptr<AbstractExpression>> children;
        children.push_back(std::move(left));
        children.push_back(std::move(right));
        return std::make_unique<ConjunctionExpression>(
            ConjunctionType::OR, std::move(children));
    }
    case hsql::kOpBetween: {
        // BETWEEN is: expr BETWEEN low AND high
        // Transform to: (expr >= low) AND (expr <= high)
        if (expr->exprList == nullptr || expr->exprList->size() != 2) {
            VELODB_THROW(ExecutionError, "BETWEEN requires exactly 2 operands");
        }

        auto low_expr = planExpression((*expr->exprList)[0]);
        auto high_expr = planExpression((*expr->exprList)[1]);

        // Create (expr >= low)
        auto left_comparison = std::make_unique<ComparisonExpression>(
            ComparisonType::GREATER_THAN_OR_EQUAL,
            planExpression(expr->expr),
            std::move(low_expr));

        // Create (expr <= high)
        auto right_comparison = std::make_unique<ComparisonExpression>(
            ComparisonType::LESS_THAN_OR_EQUAL,
            planExpression(expr->expr),
            std::move(high_expr));

        // Combine with AND
        std::vector<std::unique_ptr<AbstractExpression>> children;
        children.push_back(std::move(left_comparison));
        children.push_back(std::move(right_comparison));
        return std::make_unique<ConjunctionExpression>(
            ConjunctionType::AND, std::move(children));
    }
    case hsql::kOpIn: {
        // TODO: Implement IN operator
        VELODB_THROW(ExecutionError, "IN operator not yet implemented");
    }
    // TODO: Implement other complex operators
    default:
        VELODB_THROW(ExecutionError, "Operator not implemented: " + std::to_string(static_cast<int>(expr->opType)));
    }
}

std::vector<std::unique_ptr<AbstractExpression>> QueryPlanner::planSelectList(const std::vector<hsql::Expr*>* select_list)
{
    std::vector<std::unique_ptr<AbstractExpression>> expressions;

    if (select_list == nullptr || select_list->empty()) {
        // SELECT * case - return empty vector to indicate all columns
        return expressions;
    }

    // Check if this is SELECT * (single kExprStar expression)
    if (select_list->size() == 1 && (*select_list)[0]->type == hsql::kExprStar) {
        // SELECT * case - return empty vector to indicate all columns
        return expressions;
    }

    for (const auto* expr : *select_list) {
        if (expr->type == hsql::kExprStar) {
            VELODB_THROW(ExecutionError, "* cannot be mixed with other expressions in SELECT list");
        }
        expressions.push_back(planExpression(expr));
    }

    return expressions;
}

std::unique_ptr<Schema> QueryPlanner::inferScanFilterSchema(const Schema& input_schema)
{
    auto schema = input_schema.clone();
    schema->addColumnInfo({ "$_rowid", std::make_unique<IntegerType>(), false });
    schema->addColumnInfo({ "$_mask", std::make_unique<BooleanType>(), false });
    return schema;
}

std::unique_ptr<Schema> QueryPlanner::inferProjectionSchema(
    const std::vector<std::unique_ptr<AbstractExpression>>& expressions,
    const Schema& input_schema)
{
    if (expressions.empty()) {
        // SELECT * case - return clone of input schema
        return input_schema.clone();
    }

    // TODO: Implement proper schema inference from expressions
    // For now, return a simple schema based on expression types
    std::vector<ColumnInfo> columns;

    for (size_t i = 0; i < expressions.size(); ++i) {
        const auto& expr = expressions[i];
        std::string column_name = "col_" + std::to_string(i);
        auto column_type = DataType::createType(expr->getReturnType().getTypeId());
        columns.emplace_back(column_name, std::move(column_type), true);
    }

    return std::make_unique<Schema>(std::move(columns));
}

} // namespace velodb
