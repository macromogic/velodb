#include "planner/query_planner.hpp"

#include "catalog/catalog.hpp"
#include "catalog/column.hpp"
#include "catalog/execution_context.hpp"
#include "catalog/schema.hpp"
#include "common/exception.hpp"
#include "common/profiler.hpp"
#include "data/data_type.hpp"
#include "data/type_checker.hpp"
#include "expression/arithmetic_expression.hpp"
#include "expression/column_ref_expression.hpp"
#include "expression/comparison_expression.hpp"
#include "expression/constant_expression.hpp"
#include "expression/expression.hpp"
#include "expression/logical_expression.hpp"
#include "planner/abstract_plan_node.hpp"
#include "planner/filter_compaction_plan_node.hpp"
#include "planner/limit_plan_node.hpp"
#include "planner/materialization_plan_node.hpp"
#include "planner/merge_sort_join_plan_node.hpp"
#include "planner/projection_plan_node.hpp"
#include "planner/seq_scan_plan_node.hpp"
#include "planner/sort_plan_node.hpp"

#include <SQLParser.h>
#include <fmt/format.h>

#include <memory>

namespace hsql {

static auto format_as(OperatorType op_type)
{
    switch (op_type) {
    case OperatorType::kOpNone:
        return "NONE";
    case OperatorType::kOpBetween:
        return "BETWEEN";
    case OperatorType::kOpCase:
        return "CASE";
    case OperatorType::kOpCaseListElement:
        return "WHEN ... THEN";
    case OperatorType::kOpPlus:
        return "+";
    case OperatorType::kOpMinus:
        return "-";
    case OperatorType::kOpAsterisk:
        return "*";
    case OperatorType::kOpSlash:
        return "/";
    case OperatorType::kOpPercentage:
        return "%";
    case OperatorType::kOpCaret:
        return "^";
    case OperatorType::kOpEquals:
        return "=";
    case OperatorType::kOpNotEquals:
        return "!=";
    case OperatorType::kOpLess:
        return "<";
    case OperatorType::kOpLessEq:
        return "<=";
    case OperatorType::kOpGreater:
        return ">";
    case OperatorType::kOpGreaterEq:
        return ">=";
    case OperatorType::kOpLike:
        return "LIKE";
    case OperatorType::kOpNotLike:
        return "NOT LIKE";
    case OperatorType::kOpILike:
        return "ILIKE";
    case OperatorType::kOpAnd:
        return "AND";
    case OperatorType::kOpOr:
        return "OR";
    case OperatorType::kOpIn:
        return "IN";
    case OperatorType::kOpConcat:
        return "CONCAT";
    case OperatorType::kOpNot:
        return "NOT";
    case OperatorType::kOpUnaryMinus:
        return "-";
    case OperatorType::kOpIsNull:
        return "IS NULL";
    case OperatorType::kOpExists:
        return "EXISTS";
    }
    __builtin_unreachable();
}

} // namespace hsql

namespace velodb {

// QueryPlanner implementation
QueryPlanner::QueryPlanner(Catalog& catalog)
    : catalog_(catalog)
{
}

std::unique_ptr<AbstractPlanNode> QueryPlanner::planSelect(const hsql::SelectStatement* select_stmt)
{
    PROFILE_SCOPE("Query Planning");
    // Plan the FROM clause
    auto* table_ref = select_stmt->fromTable;
    VELODB_ASSERT_MSG(table_ref != nullptr, "SELECT without FROM not supported");

    // Plan WHERE clause and merge with scan
    std::unique_ptr<AbstractExpression> predicate = nullptr;
    if (select_stmt->whereClause != nullptr) {
        predicate = planExpression(table_ref, select_stmt->whereClause);
    }

    auto plan = planTableRef(table_ref, std::move(predicate));

    bool is_select_star = false;
    if (select_stmt->selectList && !select_stmt->selectList->empty()) {
        auto projection_expressions = planSelectList(table_ref, select_stmt->selectList);
        is_select_star = projection_expressions.empty();
        if (!projection_expressions.empty()) {
            auto table_name = table_ref->name;
            projection_expressions.push_back(
                std::make_unique<ColumnRefExpression>(table_name, "$_rowid", std::make_unique<BigIntType>()));
            projection_expressions.push_back(
                std::make_unique<ColumnRefExpression>(table_name, "$_mask", std::make_unique<BooleanType>()));
        }
        auto& input_schema = plan->getOutputSchema();
        auto projection_schema = inferProjectionSchema(projection_expressions, input_schema);
        auto projection_plan = std::make_unique<ProjectionPlanNode>(input_schema.clone(),
                                                                    std::move(projection_schema),
                                                                    std::move(projection_expressions));
        projection_plan->addChild(std::move(plan));
        plan = std::move(projection_plan);
    }

    // If this is SELECT * over a join result that currently only has rowid pair schema, inject materialization node
    if (is_select_star && plan->getOutputSchema().getColumnCount() == 2) {
        const auto& c0 = plan->getOutputSchema().getColumnInfo(0).getName();
        const auto& c1 = plan->getOutputSchema().getColumnInfo(1).getName();
        auto ends_with = [](const std::string& s, const std::string& suffix) {
            return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
        };
        if (ends_with(c0, "$_rowid") && ends_with(c1, "$_rowid")) {
            // Build disambiguated output schema: concatenate left & right table non-internal columns with table prefix
            auto strip_suffix = [](const std::string& name) {
                constexpr std::string_view suff = "$_rowid";
                if (name.size() >= suff.size() && name.substr(name.size() - suff.size()) == suff) {
                    auto base = name.substr(0, name.size() - suff.size());
                    // If the base ends with a dot (e.g. "A.$_rowid"), strip it.
                    if (!base.empty() && base.back() == '.') {
                        base.pop_back();
                    }
                    return base;
                }
                return name;
            };
            auto left_table_name = strip_suffix(c0);
            auto right_table_name = strip_suffix(c1);
            auto left_table_opt = catalog_.get().getTable(left_table_name);
            auto right_table_opt = catalog_.get().getTable(right_table_name);
            VELODB_ASSERT_MSG(left_table_opt && right_table_opt, "Invalid source table(s) for materialization");
            auto& left_table = left_table_opt->get();
            auto& right_table = right_table_opt->get();
            std::vector<ColumnInfo> final_columns;
            auto collect = [&](const Table& table) {
                for (size_t ci = 0; ci < table.getColumnCount(); ++ci) {
                    const auto& col_name = table.getColumnName(ci);
                    if (col_name == "$_rowid" || col_name == "$_mask")
                        continue;
                    final_columns.emplace_back(fmt::format("{}.{}", table.getName(), col_name),
                                               table.getColumnType(ci).cloneUnique());
                }
            };
            collect(left_table);
            collect(right_table);
            auto materialization_plan = std::make_unique<MaterializationPlanNode>(Schema(std::move(final_columns)));
            materialization_plan->addChild(std::move(plan));
            plan = std::move(materialization_plan);
        }
    }

    if (auto* order = select_stmt->order) {
        const auto& schema = plan->getOutputSchema();
        std::vector<size_t> order_indices;
        std::vector<bool> ascending_flags;
        for (auto* order_desc : *order) {
            VELODB_ASSERT_MSG(order_desc->expr->type == hsql::kExprColumnRef, "ORDER BY must be column references");
            auto* col_ref = order_desc->expr;
            auto col_index = schema.getColumnIndex(col_ref->name);
            order_indices.push_back(col_index);
            ascending_flags.push_back(order_desc->type == hsql::kOrderAsc);
        }
        auto sort_plan = std::make_unique<SortPlanNode>(plan->getOutputSchema().clone(),
                                                        std::move(order_indices),
                                                        std::move(ascending_flags));
        sort_plan->addChild(std::move(plan));
        plan = std::move(sort_plan);
    }

    if (auto* limit = select_stmt->limit) {
        auto* limit_expr = limit->limit;
        auto* offset_expr = limit->offset;
        int64_t limit_value = 0x7FFF'FFFF'FFFF'FFFF;
        int64_t offset_value = 0;
        if (limit_expr != nullptr) {
            VELODB_ASSERT_MSG(limit_expr->type == hsql::kExprLiteralInt, "LIMIT must be an integer literal");
            limit_value = limit_expr->ival;
        }
        if (offset_expr != nullptr) {
            VELODB_ASSERT_MSG(offset_expr->type == hsql::kExprLiteralInt, "OFFSET must be an integer literal");
            offset_value = offset_expr->ival;
        }
        auto limit_plan = std::make_unique<LimitPlanNode>(plan->getOutputSchema().clone(), limit_value, offset_value);
        limit_plan->addChild(std::move(plan));
        plan = std::move(limit_plan);
    }

    return plan;
}

std::unique_ptr<AbstractPlanNode> QueryPlanner::planTableRef(const hsql::TableRef* table_ref,
                                                             std::unique_ptr<AbstractExpression> predicate)
{
    switch (table_ref->type) {
    case hsql::kTableName: {
        std::string const table_name = table_ref->name;
        auto table = catalog_.get().getTable(table_name);
        if (!table) {
            VELODB_THROW(CatalogError, "Table not found: " + table_name);
        }
        auto output_schema = inferSeqScanSchema(*table);
        auto seq_scan_plan = std::make_unique<SeqScanPlanNode>(*table, output_schema.clone(), std::move(predicate));
        auto filter_compaction_plan = std::make_unique<FilterCompactionPlanNode>(std::move(output_schema));
        filter_compaction_plan->addChild(std::move(seq_scan_plan));
        return filter_compaction_plan;
    }
    case hsql::kTableJoin: {
        auto* join = table_ref->join;
        VELODB_ASSERT_MSG(join != nullptr, "Join table_ref must have join details");
        VELODB_ASSERT_MSG(join->type == hsql::kJoinInner, "Only inner joins are supported");
        return planJoin(join->left, join->right, join->condition);
    }
    case hsql::kTableSelect:
        VELODB_THROW(ExecutionError, "Subqueries not supported");
    case hsql::kTableCrossProduct:
        VELODB_THROW(ExecutionError, "Cross products not supported");
    }
    __builtin_unreachable();
}

std::unique_ptr<AbstractPlanNode> QueryPlanner::planJoin(const hsql::TableRef* left_ref,
                                                         const hsql::TableRef* right_ref,
                                                         const hsql::Expr* join_expr)
{
    // Plan left and right table references
    auto left_plan = planTableRef(left_ref, nullptr);
    auto right_plan = planTableRef(right_ref, nullptr);

    // Check if there's a join condition
    VELODB_ASSERT_MSG(join_expr != nullptr, "Join must have a condition (ON clause)");
    VELODB_ASSERT_MSG(join_expr->type == hsql::kExprOperator, "Join condition must be an operator expression");
    VELODB_ASSERT_MSG(join_expr->opType == hsql::kOpEquals, "Only equality joins are supported");

    // Plan the left and right key expressions for original schemas
    auto left_key_expr = planExpression(left_ref, join_expr->expr);
    auto right_key_expr = planExpression(right_ref, join_expr->expr2);

    // Build side projections: keep join key + $_rowid + $_mask
    auto build_side_pipeline = [&](std::unique_ptr<AbstractPlanNode> side_plan,
                                   const hsql::TableRef* side_ref,
                                   const hsql::Expr* key_expr) -> std::unique_ptr<AbstractPlanNode> {
        // Input schema after seq scan + compaction
        const auto& input_schema = side_plan->getOutputSchema();

        // Plan key expression again for projection context (clone)
        auto key_planned = planExpression(side_ref, key_expr);
        std::vector<std::unique_ptr<AbstractExpression>> proj_exprs;
        proj_exprs.push_back(std::move(key_planned));
        // Add $_rowid and $_mask columns
        proj_exprs.push_back(
            std::make_unique<ColumnRefExpression>(side_ref->name, "$_rowid", std::make_unique<BigIntType>()));
        proj_exprs.push_back(
            std::make_unique<ColumnRefExpression>(side_ref->name, "$_mask", std::make_unique<BooleanType>()));
        auto proj_schema = inferProjectionSchema(proj_exprs, input_schema);
        auto projection_plan = std::make_unique<ProjectionPlanNode>(input_schema.clone(),
                                                                    proj_schema.clone(),
                                                                    std::move(proj_exprs));
        projection_plan->addChild(std::move(side_plan));

        // Sort by first column (join key) ascending for merge sort join
        std::vector<size_t> order_indices { 0 };
        std::vector<bool> ascending { true };
        auto sort_plan = std::make_unique<SortPlanNode>(proj_schema.clone(),
                                                        std::move(order_indices),
                                                        std::move(ascending));
        sort_plan->addChild(std::move(projection_plan));
        return sort_plan;
    };

    left_plan = build_side_pipeline(std::move(left_plan), left_ref, join_expr->expr);
    right_plan = build_side_pipeline(std::move(right_plan), right_ref, join_expr->expr2);

    // Infer the output schema for the join
    auto left_table = catalog_.get().getTable(left_ref->name);
    auto right_table = catalog_.get().getTable(right_ref->name);
    if (!left_table || !right_table) {
        VELODB_THROW(CatalogError, "Cannot find tables for join");
    }
    auto join_schema = inferJoinSchema(*left_table, *right_table);

    // Create a merge sort join plan node (we can make this configurable later)
    auto join_plan_node = std::make_unique<MergeSortJoinPlanNode>(std::move(join_schema),
                                                                  std::move(left_key_expr),
                                                                  std::move(right_key_expr),
                                                                  JoinType::INNER);
    join_plan_node->addChild(std::move(left_plan));
    join_plan_node->addChild(std::move(right_plan));

    return join_plan_node;
}

std::unique_ptr<AbstractExpression> QueryPlanner::planExpression(const hsql::TableRef* table_ref,
                                                                 const hsql::Expr* expr)
{
    std::unique_ptr<AbstractExpression> result;
    switch (expr->type) {
    case hsql::kExprLiteralInt:
        if (expr->isBoolLiteral) {
            result = std::make_unique<ConstantExpression>(Value::createBoolean(expr->ival != 0));
        } else {
            result = std::make_unique<ConstantExpression>(Value::createInteger(expr->ival));
        }
        break;
    case hsql::kExprLiteralFloat:
        result = std::make_unique<ConstantExpression>(Value::createDouble(expr->fval));
        break;
    case hsql::kExprLiteralString:
        result = std::make_unique<ConstantExpression>(Value::createString(expr->name));
        break;
    case hsql::kExprLiteralNull:
        result = std::make_unique<ConstantExpression>(Value::createNull(DataTypeId::ANY));
        break;
    case hsql::kExprColumnRef:
        result = planColumnRef(table_ref, expr);
        break;
    case hsql::kExprOperator:
        result = planOperator(table_ref, expr);
        break;
    case hsql::kExprStar:
        VELODB_THROW(ExecutionError, "* expression should be handled in planSelectList, not planExpression");
    default:
        VELODB_THROW(ExecutionError, "Expression type not implemented");
    }
    VELODB_ASSERT_MSG(g_type_checker.validateExpression(result.get()), g_type_checker.getLastError());
    return result;
}

std::unique_ptr<AbstractExpression> QueryPlanner::planColumnRef(const hsql::TableRef* table_ref, const hsql::Expr* expr)
{
    std::string const column_name = expr->name;
    auto table_opt = catalog_.get().getTable(table_ref->name);
    if (!table_opt) {
        VELODB_THROW(CatalogError, "Table not found for column reference: " + column_name);
    }
    auto& table = table_opt->get();
    auto type = table.getColumnType(column_name).cloneUnique();
    // TODO: alias?
    return std::make_unique<ColumnRefExpression>(table.getName(), column_name, std::move(type));
}

std::unique_ptr<AbstractExpression> QueryPlanner::planOperator(const hsql::TableRef* table_ref, const hsql::Expr* expr)
{
    // Implement operator planning for all comparison operators
    switch (expr->opType) {
    case hsql::kOpPlus: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        auto return_type = g_type_checker.deduceArithmeticType(left->getReturnType(),
                                                               right->getReturnType(),
                                                               ArithmeticType::PLUS);
        return std::make_unique<ArithmeticExpression>(ArithmeticType::PLUS,
                                                      std::move(return_type),
                                                      std::move(left),
                                                      std::move(right));
    }
    case hsql::kOpMinus: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        auto return_type = g_type_checker.deduceArithmeticType(left->getReturnType(),
                                                               right->getReturnType(),
                                                               ArithmeticType::MINUS);
        return std::make_unique<ArithmeticExpression>(ArithmeticType::MINUS,
                                                      std::move(return_type),
                                                      std::move(left),
                                                      std::move(right));
    }
    case hsql::kOpAsterisk: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        auto return_type = g_type_checker.deduceArithmeticType(left->getReturnType(),
                                                               right->getReturnType(),
                                                               ArithmeticType::MULTIPLY);
        return std::make_unique<ArithmeticExpression>(ArithmeticType::MULTIPLY,
                                                      std::move(return_type),
                                                      std::move(left),
                                                      std::move(right));
    }
    case hsql::kOpSlash: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        auto return_type = g_type_checker.deduceArithmeticType(left->getReturnType(),
                                                               right->getReturnType(),
                                                               ArithmeticType::DIVIDE);
        return std::make_unique<ArithmeticExpression>(ArithmeticType::DIVIDE,
                                                      std::move(return_type),
                                                      std::move(left),
                                                      std::move(right));
    }
    case hsql::kOpPercentage: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        auto return_type = g_type_checker.deduceArithmeticType(left->getReturnType(),
                                                               right->getReturnType(),
                                                               ArithmeticType::MODULO);
        return std::make_unique<ArithmeticExpression>(ArithmeticType::MODULO,
                                                      std::move(return_type),
                                                      std::move(left),
                                                      std::move(right));
    }
    case hsql::kOpEquals: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        return planComparisonOperator(ComparisonType::EQUAL, std::move(left), std::move(right));
    }
    case hsql::kOpNotEquals: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        return planComparisonOperator(ComparisonType::NOT_EQUAL, std::move(left), std::move(right));
    }
    case hsql::kOpLess: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        return planComparisonOperator(ComparisonType::LESS_THAN, std::move(left), std::move(right));
    }
    case hsql::kOpLessEq: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        return planComparisonOperator(ComparisonType::LESS_THAN_OR_EQUAL, std::move(left), std::move(right));
    }
    case hsql::kOpGreater: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        return planComparisonOperator(ComparisonType::GREATER_THAN, std::move(left), std::move(right));
    }
    case hsql::kOpGreaterEq: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        return planComparisonOperator(ComparisonType::GREATER_THAN_OR_EQUAL, std::move(left), std::move(right));
    }
    case hsql::kOpAnd: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        return std::make_unique<BinaryLogicalExpression>(ConnectiveType::AND, std::move(left), std::move(right));
    }
    case hsql::kOpOr: {
        auto left = planExpression(table_ref, expr->expr);
        auto right = planExpression(table_ref, expr->expr2);
        return std::make_unique<BinaryLogicalExpression>(ConnectiveType::OR, std::move(left), std::move(right));
    }
    case hsql::kOpNot: {
        auto operand = planExpression(table_ref, expr->expr);
        return std::make_unique<LogicalNotExpression>(std::move(operand));
    }
    case hsql::kOpBetween: {
        // BETWEEN is: expr BETWEEN low AND high
        // Transform to: (expr >= low) AND (expr <= high)
        VELODB_ASSERT_MSG(expr->exprList != nullptr && expr->exprList->size() == 2,
                          "BETWEEN requires exactly 2 operands");

        auto operand = planExpression(table_ref, expr->expr);
        auto low_expr = planExpression(table_ref, (*expr->exprList)[0]);
        auto high_expr = planExpression(table_ref, (*expr->exprList)[1]);

        // Create (expr >= low)
        auto left_comparison = std::make_unique<ComparisonExpression>(ComparisonType::GREATER_THAN_OR_EQUAL,
                                                                      operand->cloneUnique(),
                                                                      std::move(low_expr));
        // Create (expr <= high)
        auto right_comparison = std::make_unique<ComparisonExpression>(ComparisonType::LESS_THAN_OR_EQUAL,
                                                                       std::move(operand),
                                                                       std::move(high_expr));
        // Combine with AND
        return std::make_unique<BinaryLogicalExpression>(ConnectiveType::AND,
                                                         std::move(left_comparison),
                                                         std::move(right_comparison));
    }
    case hsql::kOpLike:
    case hsql::kOpNotLike:
        VELODB_THROW(ExecutionError, "Partial comparison not supported");
    default:
        VELODB_THROW(ExecutionError, fmt::format("Operator '{}' not implemented", expr->opType));
    }
}

std::unique_ptr<AbstractExpression> QueryPlanner::planComparisonOperator(ComparisonType type,
                                                                         std::unique_ptr<AbstractExpression> left,
                                                                         std::unique_ptr<AbstractExpression> right)
{
    auto left_expression_type = left->getExpressionType();
    auto right_expression_type = right->getExpressionType();
    if (left_expression_type == ExpressionType::COLUMN_REF && right_expression_type == ExpressionType::CONSTANT) {
        auto* left_expr = static_cast<ColumnRefExpression*>(left.get());
        auto value = static_cast<ConstantExpression*>(right.release())->getValue();
        auto& table = catalog_.get().getTable(left_expr->getTableName())->get();
        auto& column = table.getColumn(left_expr->getColumnName());
        column.ensureOrdinal(value, type);
        right = std::make_unique<ConstantExpression>(value);
    } else if (left_expression_type == ExpressionType::CONSTANT
               && right_expression_type == ExpressionType::COLUMN_REF) {
        auto* right_expr = static_cast<ColumnRefExpression*>(right.get());
        auto value = static_cast<ConstantExpression*>(left.release())->getValue();
        auto& table = catalog_.get().getTable(right_expr->getTableName())->get();
        auto& column = table.getColumn(right_expr->getColumnName());
        column.ensureOrdinal(value, type);
        left = std::make_unique<ConstantExpression>(value);
    }
    return std::make_unique<ComparisonExpression>(type, std::move(left), std::move(right));
}

std::vector<std::unique_ptr<AbstractExpression>> QueryPlanner::planSelectList(
    const hsql::TableRef* table_ref,
    const std::vector<hsql::Expr*>* select_list)
{
    std::vector<std::unique_ptr<AbstractExpression>> expressions;

    VELODB_ASSERT_MSG(select_list != nullptr && !select_list->empty(), "SELECT list cannot be empty");

    // Check if this is SELECT * (single kExprStar expression)
    if (select_list->size() == 1 && (*select_list)[0]->type == hsql::kExprStar) {
        // SELECT * case - return empty vector to indicate all columns
        return expressions;
    }

    for (const auto* expr : *select_list) {
        VELODB_ASSERT_MSG(expr->type != hsql::kExprStar, "* can only be used alone in SELECT list");
        expressions.push_back(planExpression(table_ref, expr));
    }

    return expressions;
}

Schema QueryPlanner::inferSeqScanSchema(const Table& table)
{
    auto schema = table.getSchema().clone();
    schema.addColumnInfo({ "$_rowid", std::make_unique<BigIntType>(), false });
    schema.addColumnInfo({ "$_mask", std::make_unique<BooleanType>(), false });
    return schema;
}

Schema QueryPlanner::inferProjectionSchema(const std::vector<std::unique_ptr<AbstractExpression>>& expressions,
                                           const Schema& input_schema)
{
    if (expressions.empty()) {
        // SELECT * case - return clone of input schema
        return input_schema.clone();
    }

    std::vector<ColumnInfo> columns;
    for (size_t i = 0; i < expressions.size(); ++i) {
        const auto& expr = expressions[i];
        auto return_type = expr->getReturnType().cloneUnique();
        switch (expr->getExpressionType()) {
        case ExpressionType::COLUMN_REF:
            columns.emplace_back(static_cast<ColumnRefExpression*>(expr.get())->getColumnName(),
                                 std::move(return_type));
            break;
        case ExpressionType::CONSTANT:
        default:
            columns.emplace_back(fmt::format("col_{}", i), std::move(return_type));
            break;
        }
    }
    return Schema(std::move(columns));
}

Schema QueryPlanner::inferJoinSchema(const Table& left_table, const Table& right_table)
{
    // Join output schema (phase 1): only expose the rowid pairs from original tables.
    // Downstream materialization operator will use these rowids to fetch required columns.
    std::vector<ColumnInfo> columns;
    columns.emplace_back(fmt::format("{}.$_rowid", left_table.getName()), std::make_unique<BigIntType>());
    columns.emplace_back(fmt::format("{}.$_rowid", right_table.getName()), std::make_unique<BigIntType>());
    return Schema(std::move(columns));
}

} // namespace velodb
