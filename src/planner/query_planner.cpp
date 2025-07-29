#include "planner/query_planner.hpp"
#include "SQLParser.h"
#include "catalog/catalog.hpp"
#include "catalog/column.hpp"
#include "catalog/schema.hpp"
#include "common/exception.hpp"
#include "expression/expression.hpp"
#include "planner/planner.hpp"
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
        predicate = planExpression(select_stmt->fromTable, select_stmt->whereClause);
    }

    auto plan = planTableRef(select_stmt->fromTable, std::move(predicate));

    // Plan SELECT list (projection)
    if (select_stmt->selectList && !select_stmt->selectList->empty()) {
        auto projection_expressions = planSelectList(select_stmt->fromTable, select_stmt->selectList);
        auto& input_schema = catalog_.getTable(select_stmt->fromTable->name)->get().getSchema();
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
        auto table = catalog_.getTable(table_name);
        if (!table) {
            VELODB_THROW(CatalogError, "Table not found: " + table_name);
        }
        auto output_schema = inferScanFilterSchema(table->get().getSchema());
        auto scan_filter_plan = std::make_unique<ScanFilterPlanNode>(*table, output_schema->cloneUnique(), std::move(predicate));
        auto compaction_plan = std::make_unique<CompactionPlanNode>(std::move(output_schema));
        compaction_plan->addChild(std::move(scan_filter_plan));
        return compaction_plan;
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

std::unique_ptr<AbstractExpression> QueryPlanner::planExpression(const hsql::TableRef* table_ref, const hsql::Expr* expr)
{
    // TODO: Implement comprehensive expression planning
    switch (expr->type) {
    case hsql::kExprLiteralInt:
        if (expr->isBoolLiteral) {
            return std::make_unique<ConstantExpression>(Value::createBoolean(expr->ival != 0));
        } else {
            return std::make_unique<ConstantExpression>(Value::createInteger(expr->ival));
        }
    case hsql::kExprLiteralFloat:
        return std::make_unique<ConstantExpression>(Value::createDouble(expr->fval));
    case hsql::kExprLiteralString:
        return std::make_unique<ConstantExpression>(Value::createString(expr->name));
    case hsql::kExprColumnRef:
        return planColumnRef(table_ref, expr);
    case hsql::kExprOperator:
        return planOperator(table_ref, expr);
    case hsql::kExprStar:
        VELODB_THROW(ExecutionError, "* expression should be handled in planSelectList, not planExpression");
    default:
        VELODB_THROW(ExecutionError, "Expression type not implemented");
    }
}

std::unique_ptr<AbstractExpression> QueryPlanner::planColumnRef(const hsql::TableRef* table_ref, const hsql::Expr* expr)
{
    std::string const column_name = expr->name;
    auto table = catalog_.getTable(table_ref->name);
    if (!table) {
        VELODB_THROW(CatalogError, "Table not found for column reference: " + column_name);
    }
    auto type = table->get().getSchema().getColumnInfo(column_name).getType().cloneUnique();
    return std::make_unique<ColumnRefExpression>(expr->alias ? expr->alias : column_name, std::move(type));
}

std::unique_ptr<AbstractExpression> QueryPlanner::planOperator(const hsql::TableRef* table_ref, const hsql::Expr* expr)
{
    // Implement operator planning for all comparison operators
    switch (expr->opType) {
    case hsql::kOpPlus: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        return std::make_unique<ArithmeticExpression>(
            ArithmeticType::PLUS, std::move(left), std::move(right));
    }
    case hsql::kOpMinus: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        return std::make_unique<ArithmeticExpression>(
            ArithmeticType::MINUS, std::move(left), std::move(right));
    }
    case hsql::kOpAsterisk: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        return std::make_unique<ArithmeticExpression>(
            ArithmeticType::MULTIPLY, std::move(left), std::move(right));
    }
    case hsql::kOpSlash: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        return std::make_unique<ArithmeticExpression>(
            ArithmeticType::DIVIDE, std::move(left), std::move(right));
    }
    case hsql::kOpPercentage: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        return std::make_unique<ArithmeticExpression>(
            ArithmeticType::MODULO, std::move(left), std::move(right));
    }
    case hsql::kOpEquals: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        return std::make_unique<ComparisonExpression>(
            ComparisonType::EQUAL, std::move(left), std::move(right));
    }
    case hsql::kOpNotEquals: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        return std::make_unique<ComparisonExpression>(
            ComparisonType::NOT_EQUAL, std::move(left), std::move(right));
    }
    case hsql::kOpLess: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        return std::make_unique<ComparisonExpression>(
            ComparisonType::LESS_THAN, std::move(left), std::move(right));
    }
    case hsql::kOpLessEq: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        return std::make_unique<ComparisonExpression>(
            ComparisonType::LESS_THAN_OR_EQUAL, std::move(left), std::move(right));
    }
    case hsql::kOpGreater: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        return std::make_unique<ComparisonExpression>(
            ComparisonType::GREATER_THAN, std::move(left), std::move(right));
    }
    case hsql::kOpGreaterEq: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        return std::make_unique<ComparisonExpression>(
            ComparisonType::GREATER_THAN_OR_EQUAL, std::move(left), std::move(right));
    }
    case hsql::kOpLike: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        return std::make_unique<ComparisonExpression>(
            ComparisonType::LIKE, std::move(left), std::move(right));
    }
    case hsql::kOpNotLike: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        return std::make_unique<ComparisonExpression>(
            ComparisonType::NOT_LIKE, std::move(left), std::move(right));
    }
    case hsql::kOpAnd: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        return std::make_unique<BinaryLogicalExpression>(
            ConnectiveType::AND, std::move(left), std::move(right));
    }
    case hsql::kOpOr: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        return std::make_unique<BinaryLogicalExpression>(
            ConnectiveType::OR, std::move(left), std::move(right));
    }
    case hsql::kOpNot: {
        auto operand = planExpression(table_ref, expr->expr);
        return std::make_unique<LogicalNotExpression>(std::move(operand));
    }
    case hsql::kOpBetween: {
        // BETWEEN is: expr BETWEEN low AND high
        // Transform to: (expr >= low) AND (expr <= high)
        if (expr->exprList == nullptr || expr->exprList->size() != 2) {
            VELODB_THROW(ExecutionError, "BETWEEN requires exactly 2 operands");
        }

        auto low_expr = planExpression(table_ref, (*expr->exprList)[0]);
        auto high_expr = planExpression(table_ref, (*expr->exprList)[1]);

        // Create (expr >= low)
        auto left_comparison = std::make_unique<ComparisonExpression>(
            ComparisonType::GREATER_THAN_OR_EQUAL,
            planExpression(table_ref, expr->expr),
            std::move(low_expr));

        // Create (expr <= high)
        auto right_comparison = std::make_unique<ComparisonExpression>(
            ComparisonType::LESS_THAN_OR_EQUAL,
            planExpression(table_ref, expr->expr),
            std::move(high_expr));

        // Combine with AND
        return std::make_unique<BinaryLogicalExpression>(
            ConnectiveType::AND, std::move(left_comparison), std::move(right_comparison));
    }
    case hsql::kOpIn: {
        // TODO: Implement IN operator
        VELODB_THROW(ExecutionError, "IN operator not implemented");
    }
    // TODO: Implement other complex operators
    default:
        VELODB_THROW(ExecutionError, "Operator not implemented: " + std::to_string(static_cast<int>(expr->opType)));
    }
}

std::vector<std::unique_ptr<AbstractExpression>> QueryPlanner::planSelectList(const hsql::TableRef* table_ref, const std::vector<hsql::Expr*>* select_list)
{
    std::vector<std::unique_ptr<AbstractExpression>> expressions;

    if (select_list == nullptr || select_list->empty()) {
        VELODB_THROW(ExecutionError, "SELECT list cannot be empty");
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
        expressions.push_back(planExpression(table_ref, expr));
    }

    return expressions;
}

std::unique_ptr<Schema> QueryPlanner::inferScanFilterSchema(const Schema& input_schema)
{
    auto schema = input_schema.cloneUnique();
    schema->addColumnInfo({ "$_rowid", std::make_unique<BigIntType>(), false });
    schema->addColumnInfo({ "$_mask", std::make_unique<BooleanType>(), false });
    return schema;
}

std::unique_ptr<Schema> QueryPlanner::inferProjectionSchema(
    const std::vector<std::unique_ptr<AbstractExpression>>& expressions,
    const Schema& input_schema)
{
    if (expressions.empty()) {
        // SELECT * case - return clone of input schema
        return input_schema.cloneUnique();
    }

    std::vector<ColumnInfo> columns;
    for (size_t i = 0; i < expressions.size(); ++i) {
        const auto& expr = expressions[i];
        auto return_type = expr->getReturnType().cloneUnique();
        switch (expr->getExpressionType()) {
        case ExpressionType::COLUMN_REF:
            columns.emplace_back(static_cast<ColumnRefExpression*>(expr.get())->getColumnName(), std::move(return_type));
            break;
        case ExpressionType::CONSTANT:
        default:
            columns.emplace_back("col_" + std::to_string(i), std::move(return_type));
            break;
        }
    }
    return std::make_unique<Schema>(std::move(columns));
}

} // namespace velodb
