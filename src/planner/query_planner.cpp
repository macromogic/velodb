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
    VELODB_ASSERT_MSG(select_stmt->selectList != nullptr && !select_stmt->selectList->empty(),
                      "SELECT without select list not supported");

    // Plan WHERE clause and merge with scan
    std::unique_ptr<AbstractExpression> predicate = nullptr;
    if (select_stmt->whereClause != nullptr) {
        predicate = parseExpression(table_ref, select_stmt->whereClause);
    }

    auto plan = planTableRef(table_ref, std::move(predicate));

    auto projection_expressions = parseSelectList(table_ref, select_stmt->selectList);
    auto& input_schema = plan->getOutputSchema();
    auto select_schema = inferSelectSchema(projection_expressions);
    switch (table_ref->type) {
    case hsql::kTableName: {
        auto projection_plan = std::make_unique<ProjectionPlanNode>(input_schema.clone(),
                                                                    std::move(select_schema),
                                                                    std::move(projection_expressions));
        projection_plan->addChild(std::move(plan));
        plan = std::move(projection_plan);
        break;
    }
    case hsql::kTableJoin: {
        auto materialization_plan = std::make_unique<MaterializationPlanNode>(std::move(select_schema));
        materialization_plan->addChild(std::move(plan));
        plan = std::move(materialization_plan);
        break;
    }
    default:
        VELODB_THROW(ExecutionError, "Unsupported table ref type for select list planning");
    }

    if (auto* orders = select_stmt->order) {
        plan = planOrderBy(std::move(plan), orders);
    }

    if (auto* limit_desc = select_stmt->limit) {
        plan = planLimitOffset(std::move(plan), limit_desc);
    }

    return plan;
}

std::unique_ptr<AbstractPlanNode> QueryPlanner::planTableRef(const hsql::TableRef* table_ref,
                                                             std::unique_ptr<AbstractExpression> predicate)
{
    switch (table_ref->type) {
    case hsql::kTableName: {
        const std::string table_name = table_ref->name;
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
        auto* join_expr = join->condition;
        // Check if there's a join condition
        VELODB_ASSERT_MSG(join_expr != nullptr, "Join must have a condition (ON clause)");
        VELODB_ASSERT_MSG(join_expr->type == hsql::kExprOperator, "Join condition must be an operator expression");
        VELODB_ASSERT_MSG(join_expr->opType == hsql::kOpEquals, "Only equality joins are supported");

        // Plan the left and right key expressions for original schemas
        auto key_expr_1 = parseExpression(table_ref, join_expr->expr);
        auto key_expr_2 = parseExpression(table_ref, join_expr->expr2);
        if (expressionReferencesOnlyTable(key_expr_1.get(), join->left->name)) {
            return planEquiJoin(join->left,
                               join->right,
                               std::move(key_expr_1),
                               std::move(key_expr_2),
                               std::move(predicate));
        } else {
            return planEquiJoin(join->left,
                               join->right,
                               std::move(key_expr_2),
                               std::move(key_expr_1),
                               std::move(predicate));
        }
    }
    case hsql::kTableSelect:
        VELODB_THROW(ExecutionError, "Subqueries not supported");
    case hsql::kTableCrossProduct:
        VELODB_THROW(ExecutionError, "Cross products not supported");
    }
    __builtin_unreachable();
}

std::unique_ptr<AbstractPlanNode> QueryPlanner::planEquiJoin(const hsql::TableRef* left_ref,
                                                             const hsql::TableRef* right_ref,
                                                             std::unique_ptr<AbstractExpression> left_key_expr,
                                                             std::unique_ptr<AbstractExpression> right_key_expr,
                                                             std::unique_ptr<AbstractExpression> predicate)
{
    VELODB_ASSERT_MSG(left_ref != nullptr && left_ref->type == hsql::kTableName, "Left table_ref must be a table name");
    VELODB_ASSERT_MSG(right_ref != nullptr && right_ref->type == hsql::kTableName,
                      "Right table_ref must be a table name");
    // Separate predicate into left and right table predicates
    std::unique_ptr<AbstractExpression> left_predicate = nullptr;
    std::unique_ptr<AbstractExpression> right_predicate = nullptr;

    if (predicate) {
        // Extract all conjunctive clauses
        std::vector<const AbstractExpression*> conjuncts;
        extractConjuncts(predicate.get(), conjuncts);

        // Separate conjuncts by table
        std::vector<std::unique_ptr<AbstractExpression>> left_conjuncts;
        std::vector<std::unique_ptr<AbstractExpression>> right_conjuncts;

        for (const auto* conjunct : conjuncts) {
            if (expressionReferencesOnlyTable(conjunct, left_ref->name)) {
                left_conjuncts.push_back(conjunct->cloneUnique());
            } else if (expressionReferencesOnlyTable(conjunct, right_ref->name)) {
                right_conjuncts.push_back(conjunct->cloneUnique());
            }
            // Note: cross-table predicates are ignored as per user's assumption
        }

        // Reconstruct predicates from conjuncts
        if (!left_conjuncts.empty()) {
            left_predicate = std::move(left_conjuncts[0]);
            for (size_t i = 1; i < left_conjuncts.size(); ++i) {
                left_predicate = std::make_unique<BinaryLogicalExpression>(ConnectiveType::AND,
                                                                           std::move(left_predicate),
                                                                           std::move(left_conjuncts[i]));
            }
        }

        if (!right_conjuncts.empty()) {
            right_predicate = std::move(right_conjuncts[0]);
            for (size_t i = 1; i < right_conjuncts.size(); ++i) {
                right_predicate = std::make_unique<BinaryLogicalExpression>(ConnectiveType::AND,
                                                                            std::move(right_predicate),
                                                                            std::move(right_conjuncts[i]));
            }
        }
    }

    // Plan left and right table references with separated predicates
    auto left_plan = planJoinSide(left_ref, left_key_expr->cloneUnique(), std::move(left_predicate));
    auto right_plan = planJoinSide(right_ref, right_key_expr->cloneUnique(), std::move(right_predicate));

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

std::unique_ptr<AbstractPlanNode> QueryPlanner::planOrderBy(std::unique_ptr<AbstractPlanNode>&& plan,
                                                            const std::vector<hsql::OrderDescription*>* orders)
{
    const auto& schema = plan->getOutputSchema();
    std::vector<size_t> order_indices;
    std::vector<bool> ascending_flags;
    for (auto* order_desc : *orders) {
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
    return sort_plan;
}

std::unique_ptr<AbstractPlanNode> QueryPlanner::planLimitOffset(std::unique_ptr<AbstractPlanNode>&& plan,
                                                                const hsql::LimitDescription* limit_desc)
{
    auto* limit_expr = limit_desc->limit;
    auto* offset_expr = limit_desc->offset;
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
    return limit_plan;
}

std::unique_ptr<AbstractPlanNode> QueryPlanner::planJoinSide(const hsql::TableRef* table_ref,
                                                         std::unique_ptr<AbstractExpression> join_key_expr,
                                                         std::unique_ptr<AbstractExpression> predicate)
{
    auto plan = planTableRef(table_ref, std::move(predicate));
    // Input schema after seq scan + compaction
    const auto& input_schema = plan->getOutputSchema();

    // Plan key expression again for projection context (clone)
    std::vector<std::unique_ptr<AbstractExpression>> proj_exprs;
    proj_exprs.push_back(std::move(join_key_expr));
    // Add $_rowid and $_mask columns
    proj_exprs.push_back(
        std::make_unique<ColumnRefExpression>(table_ref->name, "$_rowid", std::make_unique<BigIntType>()));
    proj_exprs.push_back(
        std::make_unique<ColumnRefExpression>(table_ref->name, "$_mask", std::make_unique<BooleanType>()));
    auto proj_schema = inferSelectSchema(proj_exprs);
    auto projection_plan = std::make_unique<ProjectionPlanNode>(input_schema.clone(),
                                                                proj_schema.clone(),
                                                                std::move(proj_exprs));
    projection_plan->addChild(std::move(plan));

    // Sort by first column (join key) ascending for merge sort join
    std::vector<size_t> order_indices { 0 };
    std::vector<bool> ascending { true };
    auto sort_plan = std::make_unique<SortPlanNode>(proj_schema.clone(),
                                                    std::move(order_indices),
                                                    std::move(ascending));
    sort_plan->addChild(std::move(projection_plan));
    return sort_plan;
}

Schema QueryPlanner::inferSeqScanSchema(const Table& table)
{
    auto schema = table.getSchema().clone();
    schema.addColumnInfo({ "$_rowid", std::make_unique<BigIntType>(), false });
    schema.addColumnInfo({ "$_mask", std::make_unique<BooleanType>(), false });
    return schema;
}

Schema QueryPlanner::inferSelectSchema(const std::vector<std::unique_ptr<AbstractExpression>>& expressions)
{
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

std::vector<std::unique_ptr<AbstractExpression>> QueryPlanner::parseSelectList(
    const hsql::TableRef* table_ref,
    const std::vector<hsql::Expr*>* select_list)
{
    std::vector<std::unique_ptr<AbstractExpression>> expressions;

    VELODB_ASSERT_MSG(select_list != nullptr && !select_list->empty(), "SELECT list cannot be empty");

    // Check if this is SELECT * (single kExprStar expression)
    if (select_list->size() == 1 && (*select_list)[0]->type == hsql::kExprStar) {
        switch (table_ref->type) {
        case hsql::kTableName: {
            const std::string table_name = table_ref->name;
            auto& schema = catalog_.get().getTable(table_name)->get().getSchema();
            for (const auto& col_info : schema) {
                expressions.push_back(std::make_unique<ColumnRefExpression>(table_name,
                                                                            col_info.getName(),
                                                                            col_info.getType().cloneUnique()));
            }
            break;
        }
        case hsql::kTableJoin: {
            const std::string left_name = table_ref->join->left->name;
            const std::string right_name = table_ref->join->right->name;
            auto& left_schema = catalog_.get().getTable(left_name)->get().getSchema();
            for (const auto& col_info : left_schema) {
                expressions.push_back(std::make_unique<ColumnRefExpression>(left_name,
                                                                            col_info.getName(),
                                                                            col_info.getType().cloneUnique()));
            }
            auto& right_schema = catalog_.get().getTable(right_name)->get().getSchema();
            for (const auto& col_info : right_schema) {
                expressions.push_back(std::make_unique<ColumnRefExpression>(right_name,
                                                                            col_info.getName(),
                                                                            col_info.getType().cloneUnique()));
            }
            break;
        }
        default:
            VELODB_THROW(ExecutionError, "Unsupported table reference type for SELECT *");
        }
    } else {
        for (const auto* expr : *select_list) {
            VELODB_ASSERT_MSG(expr->type != hsql::kExprStar, "* can only be used alone in SELECT list");
            expressions.push_back(parseExpression(table_ref, expr));
        }
    }

    return expressions;
}

std::unique_ptr<AbstractExpression> QueryPlanner::parseExpression(const hsql::TableRef* table_ref,
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
    case hsql::kExprLiteralDate:
        result = std::make_unique<ConstantExpression>(Value::createDate(expr->name));
        break;
    case hsql::kExprLiteralNull:
        result = std::make_unique<ConstantExpression>(Value::createNull(DataTypeId::ANY));
        break;
    case hsql::kExprColumnRef:
        result = parseColumnRef(table_ref, expr);
        if (!result) {
            VELODB_THROW(ExecutionError, fmt::format("Column reference not found: {}", expr->name));
        }
        break;
    case hsql::kExprOperator:
        result = parseOperator(table_ref, expr);
        break;
    case hsql::kExprStar:
        VELODB_THROW(ExecutionError, "* expression should be handled in parseSelectList, not parseExpression");
    default:
        VELODB_THROW(ExecutionError, "Expression type not implemented");
    }
    VELODB_ASSERT_MSG(g_type_checker.validateExpression(result.get()), g_type_checker.getLastError());
    return result;
}

std::unique_ptr<AbstractExpression> QueryPlanner::parseColumnRef(const hsql::TableRef* table_ref,
                                                                 const hsql::Expr* expr)
{
    switch (table_ref->type) {
    case hsql::kTableName: {
        const std::string table_name = table_ref->name;
        if (expr->table != nullptr && table_name != expr->table) {
            return nullptr;
        }
        auto table_opt = catalog_.get().getTable(table_name);
        VELODB_ASSERT_MSG(table_opt, "Table not found in catalog: " + table_name);
        auto& table = table_opt->get();
        const std::string column_name = expr->name;
        if (!table.hasColumn(column_name)) {
            return nullptr;
        }
        auto type = table.getColumnType(column_name).cloneUnique();
        // TODO: alias?
        return std::make_unique<ColumnRefExpression>(table.getName(), column_name, std::move(type));
    }
    case hsql::kTableJoin: {
        auto left_expr = parseColumnRef(table_ref->join->left, expr);
        if (left_expr) {
            return left_expr;
        }
        auto right_expr = parseColumnRef(table_ref->join->right, expr);
        return right_expr;
    }
    case hsql::kTableSelect:
    case hsql::kTableCrossProduct:
        VELODB_THROW(ExecutionError, "Unsupported table reference type for column reference");
    }
    __builtin_unreachable();
}

std::unique_ptr<AbstractExpression> QueryPlanner::parseOperator(const hsql::TableRef* table_ref, const hsql::Expr* expr)
{
    // Implement operator planning for all comparison operators
    switch (expr->opType) {
    case hsql::kOpPlus: {
        auto left = parseExpression(table_ref, expr->expr);
        auto right = parseExpression(table_ref, expr->expr2);
        auto return_type = g_type_checker.deduceArithmeticType(left->getReturnType(),
                                                               right->getReturnType(),
                                                               ArithmeticType::PLUS);
        return std::make_unique<ArithmeticExpression>(ArithmeticType::PLUS,
                                                      std::move(return_type),
                                                      std::move(left),
                                                      std::move(right));
    }
    case hsql::kOpMinus: {
        auto left = parseExpression(table_ref, expr->expr);
        auto right = parseExpression(table_ref, expr->expr2);
        auto return_type = g_type_checker.deduceArithmeticType(left->getReturnType(),
                                                               right->getReturnType(),
                                                               ArithmeticType::MINUS);
        return std::make_unique<ArithmeticExpression>(ArithmeticType::MINUS,
                                                      std::move(return_type),
                                                      std::move(left),
                                                      std::move(right));
    }
    case hsql::kOpAsterisk: {
        auto left = parseExpression(table_ref, expr->expr);
        auto right = parseExpression(table_ref, expr->expr2);
        auto return_type = g_type_checker.deduceArithmeticType(left->getReturnType(),
                                                               right->getReturnType(),
                                                               ArithmeticType::MULTIPLY);
        return std::make_unique<ArithmeticExpression>(ArithmeticType::MULTIPLY,
                                                      std::move(return_type),
                                                      std::move(left),
                                                      std::move(right));
    }
    case hsql::kOpSlash: {
        auto left = parseExpression(table_ref, expr->expr);
        auto right = parseExpression(table_ref, expr->expr2);
        auto return_type = g_type_checker.deduceArithmeticType(left->getReturnType(),
                                                               right->getReturnType(),
                                                               ArithmeticType::DIVIDE);
        return std::make_unique<ArithmeticExpression>(ArithmeticType::DIVIDE,
                                                      std::move(return_type),
                                                      std::move(left),
                                                      std::move(right));
    }
    case hsql::kOpPercentage: {
        auto left = parseExpression(table_ref, expr->expr);
        auto right = parseExpression(table_ref, expr->expr2);
        auto return_type = g_type_checker.deduceArithmeticType(left->getReturnType(),
                                                               right->getReturnType(),
                                                               ArithmeticType::MODULO);
        return std::make_unique<ArithmeticExpression>(ArithmeticType::MODULO,
                                                      std::move(return_type),
                                                      std::move(left),
                                                      std::move(right));
    }
    case hsql::kOpEquals: {
        auto left = parseExpression(table_ref, expr->expr);
        auto right = parseExpression(table_ref, expr->expr2);
        return createComparisonOperator(ComparisonType::EQUAL, std::move(left), std::move(right));
    }
    case hsql::kOpNotEquals: {
        auto left = parseExpression(table_ref, expr->expr);
        auto right = parseExpression(table_ref, expr->expr2);
        return createComparisonOperator(ComparisonType::NOT_EQUAL, std::move(left), std::move(right));
    }
    case hsql::kOpLess: {
        auto left = parseExpression(table_ref, expr->expr);
        auto right = parseExpression(table_ref, expr->expr2);
        return createComparisonOperator(ComparisonType::LESS_THAN, std::move(left), std::move(right));
    }
    case hsql::kOpLessEq: {
        auto left = parseExpression(table_ref, expr->expr);
        auto right = parseExpression(table_ref, expr->expr2);
        return createComparisonOperator(ComparisonType::LESS_THAN_OR_EQUAL, std::move(left), std::move(right));
    }
    case hsql::kOpGreater: {
        auto left = parseExpression(table_ref, expr->expr);
        auto right = parseExpression(table_ref, expr->expr2);
        return createComparisonOperator(ComparisonType::GREATER_THAN, std::move(left), std::move(right));
    }
    case hsql::kOpGreaterEq: {
        auto left = parseExpression(table_ref, expr->expr);
        auto right = parseExpression(table_ref, expr->expr2);
        return createComparisonOperator(ComparisonType::GREATER_THAN_OR_EQUAL, std::move(left), std::move(right));
    }
    case hsql::kOpAnd: {
        auto left = parseExpression(table_ref, expr->expr);
        auto right = parseExpression(table_ref, expr->expr2);
        return std::make_unique<BinaryLogicalExpression>(ConnectiveType::AND, std::move(left), std::move(right));
    }
    case hsql::kOpOr: {
        auto left = parseExpression(table_ref, expr->expr);
        auto right = parseExpression(table_ref, expr->expr2);
        return std::make_unique<BinaryLogicalExpression>(ConnectiveType::OR, std::move(left), std::move(right));
    }
    case hsql::kOpNot: {
        auto operand = parseExpression(table_ref, expr->expr);
        return std::make_unique<LogicalNotExpression>(std::move(operand));
    }
    case hsql::kOpBetween: {
        // BETWEEN is: expr BETWEEN low AND high
        // Transform to: (expr >= low) AND (expr <= high)
        VELODB_ASSERT_MSG(expr->exprList != nullptr && expr->exprList->size() == 2,
                          "BETWEEN requires exactly 2 operands");

        auto operand = parseExpression(table_ref, expr->expr);
        auto low_expr = parseExpression(table_ref, (*expr->exprList)[0]);
        auto high_expr = parseExpression(table_ref, (*expr->exprList)[1]);

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
    case hsql::kOpIn: {
        // IN is: (expr = val1) OR (expr = val2) OR ...
        VELODB_ASSERT_MSG(expr->exprList != nullptr && !expr->exprList->empty(),
                          "IN requires a non-empty list of operands");
        auto operand = parseExpression(table_ref, expr->expr);
        std::unique_ptr<AbstractExpression> in_expression = nullptr;
        for (const auto* list_expr : *expr->exprList) {
            auto value_expr = parseExpression(table_ref, list_expr);
            auto equality_expr = createComparisonOperator(ComparisonType::EQUAL,
                                                          operand->cloneUnique(),
                                                          std::move(value_expr));
            if (in_expression == nullptr) {
                in_expression = std::move(equality_expr);
            } else {
                in_expression = std::make_unique<BinaryLogicalExpression>(ConnectiveType::OR,
                                                                          std::move(in_expression),
                                                                          std::move(equality_expr));
            }
        }
        return in_expression;
    }
    case hsql::kOpLike:
    case hsql::kOpNotLike:
        VELODB_THROW(ExecutionError, "Partial comparison not supported");
    default:
        VELODB_THROW(ExecutionError, fmt::format("Operator '{}' not implemented", expr->opType));
    }
}

std::unique_ptr<AbstractExpression> QueryPlanner::createComparisonOperator(ComparisonType type,
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

void QueryPlanner::extractConjuncts(const AbstractExpression* expr, std::vector<const AbstractExpression*>& conjuncts)
{
    if (!expr) {
        return;
    }
    // If this is an AND expression, recursively extract from both sides
    if (expr->getExpressionType() == ExpressionType::LOGICAL
        && static_cast<const BinaryLogicalExpression*>(expr)->getConnectiveType() == ConnectiveType::AND) {
        const auto* logical_expr = static_cast<const BinaryLogicalExpression*>(expr);
        extractConjuncts(&logical_expr->getLeftExpression(), conjuncts);
        extractConjuncts(&logical_expr->getRightExpression(), conjuncts);
    } else {
        // This is a leaf predicate
        conjuncts.push_back(expr);
    }
}

bool QueryPlanner::expressionReferencesOnlyTable(const AbstractExpression* expr, const std::string_view table_name)
{
    if (expr->isLeaf()) {
        if (expr->getExpressionType() == ExpressionType::COLUMN_REF) {
            const auto* col_ref = static_cast<const ColumnRefExpression*>(expr);
            const auto& ref_table_name = col_ref->getTableName();
            return table_name == ref_table_name;
        } else {
            // Non-column leaf nodes do not reference any table
            return true;
        }
    } else if (expr->isUnary()) {
        const auto& child = static_cast<const UnaryExpression*>(expr)->getOperandExpression();
        return expressionReferencesOnlyTable(&child, table_name);
    } else {
        // expr must be binary
        const auto& left = static_cast<const BinaryExpression*>(expr)->getLeftExpression();
        const auto& right = static_cast<const BinaryExpression*>(expr)->getRightExpression();
        return expressionReferencesOnlyTable(&left, table_name) && expressionReferencesOnlyTable(&right, table_name);
    }
}

} // namespace velodb
