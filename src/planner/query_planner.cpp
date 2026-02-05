#include "planner/query_planner.hpp"

#include "catalog/catalog.hpp"
#include "catalog/column.hpp"
#include "catalog/execution_context.hpp"
#include "catalog/schema.hpp"
#include "common/constants.hpp"
#include "common/exception.hpp"
#include "common/profiler.hpp"
#include "common/string_utils.hpp"
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
#include "planner/gpu_filter_plan_node.hpp"
#include "planner/hash_join_plan_node.hpp"
#include "planner/limit_plan_node.hpp"
#include "planner/materialization_plan_node.hpp"
#include "planner/projection_plan_node.hpp"
#include "planner/seq_scan_plan_node.hpp"
#include "planner/sort_merge_join_plan_node.hpp"
#include "planner/sort_plan_node.hpp"

#include <SQLParser.h>
#include <fmt/format.h>

#include <list>
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
QueryPlanner::QueryPlanner(Catalog& catalog, JoinStrategy join_strategy)
    : catalog_(catalog)
    , join_strategy_(join_strategy)
{
}

std::unique_ptr<AbstractPlanNode> QueryPlanner::planSelect(const hsql::SelectStatement* select_stmt)
{
    // Plan FROM clause
    auto* table_ref = select_stmt->fromTable;
    VELODB_ASSERT_MSG(table_ref != nullptr, "SELECT without FROM not supported");
    VELODB_ASSERT_MSG(select_stmt->selectList != nullptr && !select_stmt->selectList->empty(),
                      "SELECT without select list not supported");

    // Plan WHERE clause
    std::unique_ptr<AbstractExpression> predicate = nullptr;
    if (select_stmt->whereClause != nullptr) {
        predicate = parseExpression(table_ref, select_stmt->whereClause);
    }
    auto plan = planTables(table_ref, std::move(predicate));

    // The rest of planSelect (projection, order, limit) remains mostly valid
    // BUT projection expects specific columns.
    // The planTables produces a wide table with all columns from all joined tables (plus RowIds).
    // We need to ensure the columns are named correctly "TableName.ColName".

    auto output_expressions = parseSelectList(table_ref, select_stmt->selectList);
    auto& input_schema = plan->getOutputSchema();
    auto select_schema = inferSelectSchema(output_expressions);

    bool is_join = ((static_cast<uint16_t>(plan->getPlanType()) & static_cast<uint16_t>(PlanType::JOIN)) != 0);

    if (is_join) {
        // Create Projection to keep only RowIDs before Materialization
        std::vector<std::unique_ptr<AbstractExpression>> rowid_exprs;
        std::vector<ColumnInfo> rowid_cols;

        const auto& schema = plan->getOutputSchema();
        for (size_t i = 0; i < schema.getColumnCount(); ++i) {
            const auto& col = schema.getColumnInfo(i);
            const auto& name = col.getName();
            const auto& [_, col_name] = splitName(name);
            if (col_name == "$_rowid") {
                rowid_exprs.push_back(std::make_unique<ColumnRefExpression>("", name, col.getType().cloneUnique()));
                rowid_cols.push_back(col.clone());
            }
        }

        // Get rowids, then materialize
        Schema rowid_schema(std::move(rowid_cols));
        auto rowid_proj = std::make_unique<ProjectionPlanNode>(input_schema.clone(),
                                                               rowid_schema.clone(),
                                                               std::move(rowid_exprs));
        rowid_proj->addChild(std::move(plan));
        auto materialization_plan = std::make_unique<MaterializationPlanNode>(std::move(select_schema));
        materialization_plan->addChild(std::move(rowid_proj));
        plan = std::move(materialization_plan);
    } else {
        // Single table path: Standard Projection
        auto projection_plan = std::make_unique<ProjectionPlanNode>(input_schema.clone(),
                                                                    std::move(select_schema),
                                                                    std::move(output_expressions));
        projection_plan->addChild(std::move(plan));
        plan = std::move(projection_plan);
    }

    if (auto* orders = select_stmt->order) {
        plan = planOrderBy(std::move(plan), orders);
    }

    if (auto* limit_desc = select_stmt->limit) {
        plan = planLimitOffset(std::move(plan), limit_desc);
    }

    return plan;
}

std::unique_ptr<AbstractPlanNode> QueryPlanner::planTables(const hsql::TableRef* root_table_ref,
                                                           std::unique_ptr<AbstractExpression> where_predicate)
{
    // 1. Flatten TableRefs and Collect Join Conditions
    std::vector<const hsql::TableRef*> leaf_tables;
    std::vector<std::unique_ptr<AbstractExpression>> join_predicates;
    collectTableRefs(root_table_ref, leaf_tables, join_predicates);

    // Collect leaf table names (or aliases)
    std::vector<std::string_view> leaf_names;
    leaf_names.reserve(leaf_tables.size());
    for (const auto* ref : leaf_tables) {
        std::string_view name = ref->alias ? ref->alias->name : ref->name;
        leaf_names.push_back(name);
    }

    // Split WHERE clause predicates
    std::vector<std::unique_ptr<AbstractExpression>> all_predicates;
    if (where_predicate) {
        std::vector<const AbstractExpression*> conjuncts;
        extractConjuncts(where_predicate.get(), conjuncts);
        for (auto* c : conjuncts) {
            all_predicates.push_back(c->cloneUnique());
        }
    }

    // 2. Classify Predicates (Local Filter vs Join)
    std::vector<std::vector<std::unique_ptr<AbstractExpression>>> table_filters(leaf_tables.size());
    for (auto&& pred : all_predicates) {
        std::unordered_set<std::string_view> referenced_table_names;
        extractTablesFromExpression(pred.get(), referenced_table_names);
        VELODB_ASSERT_MSG(referenced_table_names.size() <= 2, "Only supports predicates referencing up to 2 tables");
        if (referenced_table_names.size() == 1) {
            // Only one table referenced, push down predicate
            const auto& table_name = *referenced_table_names.begin();
            auto it = std::find(leaf_names.begin(), leaf_names.end(), table_name);
            VELODB_ASSERT_MSG(it != leaf_names.end(), "Referenced table not found in leaf tables");
            size_t table_idx = std::distance(leaf_names.begin(), it);
            table_filters[table_idx].push_back(std::move(pred));
        } else {
            // Two tables referenced, treat as join predicate
            join_predicates.push_back(std::move(pred));
        }
    }

    // 3. Create Leaf Plans
    std::list<JoinNodeInfo> active_plans;
    for (size_t i = 0; i < leaf_tables.size(); ++i) {
        active_plans.push_back(planTableLeaf(leaf_tables[i], table_filters[i]));
    }
    VELODB_ASSERT_MSG(!active_plans.empty(), "No tables in query");

    // 4. Greedy Join Loop
    if (active_plans.size() == 1) {
        // For single table, always add FilterCompaction (no join to handle mask)
        auto& node_info = active_plans.front();
        auto filter = std::make_unique<FilterCompactionPlanNode>(std::move(node_info.seq_scan_schema));
        filter->addChild(std::move(node_info.plan));
        return filter;
    } else {
        // 4a. Pre-process: Collect all join keys for each table (deduplicated)
        std::unordered_map<std::string, std::unordered_set<std::string>> table_join_key_set;
        for (const auto& pred : join_predicates) {
            if (!pred) {
                continue;
            }
            VELODB_ASSERT_MSG(pred->getExpressionType() == ExpressionType::COMPARISON,
                              "Join predicate must be a comparison expression");
            auto* compare = static_cast<const ComparisonExpression*>(pred.get());
            VELODB_ASSERT_MSG(compare->getComparisonType() == ComparisonType::EQUAL,
                              "Only Equi-Join supported in this planner");

            auto* left_expr = &compare->getLeftExpression();
            auto* right_expr = &compare->getRightExpression();
            VELODB_ASSERT_MSG(left_expr->getExpressionType() == ExpressionType::COLUMN_REF,
                              "Join key must be column reference");
            VELODB_ASSERT_MSG(right_expr->getExpressionType() == ExpressionType::COLUMN_REF,
                              "Join key must be column reference");

            auto* left_col = static_cast<const ColumnRefExpression*>(left_expr);
            auto* right_col = static_cast<const ColumnRefExpression*>(right_expr);

            // Extract alias and column name from qualified name "alias.column"
            auto [left_alias, left_name] = splitName(left_col->getColumnName());
            auto [right_alias, right_name] = splitName(right_col->getColumnName());

            table_join_key_set[std::string(left_alias)].insert(std::string(left_name));
            table_join_key_set[std::string(right_alias)].insert(std::string(right_name));
        }

        // 4b. Add projection to each leaf node: join keys + $_rowid (+ $_mask for hash join)
        for (auto& node_info : active_plans) {
            const auto& alias = node_info.table_aliases.front();
            auto it = table_join_key_set.find(alias);
            VELODB_ASSERT_MSG(it != table_join_key_set.end(), fmt::format("Join keys not found for table {}", alias));
            const auto& keys = it->second;

            auto rowid_col_name = fmt::format("{}.$_rowid", alias);
            auto mask_col_name = fmt::format("{}.$_mask", alias);
            auto& in_schema = node_info.plan->getOutputSchema();

            // For sort-merge join, add FilterCompaction before projection (mask will be dropped)
            if (join_strategy_ != JoinStrategy::HASH_JOIN) {
                auto filter = std::make_unique<FilterCompactionPlanNode>(std::move(node_info.seq_scan_schema));
                filter->addChild(std::move(node_info.plan));
                node_info.plan = std::move(filter);
            }

            std::vector<std::unique_ptr<AbstractExpression>> proj_exprs;
            std::vector<ColumnInfo> proj_cols;
            proj_exprs.reserve(keys.size() + 2);
            proj_cols.reserve(keys.size() + 2);

            // Add all join key columns
            for (const auto& key : keys) {
                auto qualified_name = fmt::format("{}.{}", alias, key);
                const auto& col_info = in_schema.getColumnInfo(qualified_name);
                proj_exprs.push_back(
                    std::make_unique<ColumnRefExpression>("", qualified_name, col_info.getType().cloneUnique()));
                proj_cols.push_back(col_info.clone());
            }

            // Add $_rowid column
            const auto& rowid_col_info = in_schema.getColumnInfo(rowid_col_name);
            proj_exprs.push_back(
                std::make_unique<ColumnRefExpression>("", rowid_col_name, DataType::createType(DataTypeId::BIGINT)));
            proj_cols.push_back(rowid_col_info.clone());

            // For hash join, also add $_mask column (will be consumed by hash join operator)
            if (join_strategy_ == JoinStrategy::HASH_JOIN) {
                const auto& mask_col_info = in_schema.getColumnInfo(mask_col_name);
                proj_exprs.push_back(std::make_unique<ColumnRefExpression>("",
                                                                           mask_col_name,
                                                                           DataType::createType(DataTypeId::BOOLEAN)));
                proj_cols.push_back(mask_col_info.clone());
            }

            auto proj_plan = std::make_unique<ProjectionPlanNode>(in_schema.clone(),
                                                                  Schema(std::move(proj_cols)),
                                                                  std::move(proj_exprs));
            proj_plan->addChild(std::move(node_info.plan));
            node_info.plan = std::move(proj_plan);
        }

        // 4c. Greedy join loop
        JoinNodeInfo root_node = std::move(active_plans.front());
        active_plans.pop_front();
        while (!active_plans.empty()) {
            std::unique_ptr<AbstractExpression> best_pred = nullptr;
            bool left_is_left_operand = true; // Does Predicate Left Operand match Current Node?

            // Find a candidate table to join
            auto it = active_plans.begin();
            for (; it != active_plans.end(); ++it) {
                for (auto& pred : join_predicates) {
                    if (!pred) {
                        continue;
                    }
                    VELODB_ASSERT_MSG(pred->getExpressionType() == ExpressionType::COMPARISON,
                                      "Join predicate must be a comparison expression");
                    auto* compare = static_cast<const ComparisonExpression*>(pred.get());
                    VELODB_ASSERT_MSG(compare->getComparisonType() == ComparisonType::EQUAL,
                                      "Only Equi-Join supported in this planner");

                    bool left_in_root = expressionReferencesTables(&compare->getLeftExpression(),
                                                                   root_node.table_aliases);
                    bool right_in_next = expressionReferencesTables(&compare->getRightExpression(), it->table_aliases);
                    if (left_in_root && right_in_next) {
                        best_pred = std::move(pred);
                        left_is_left_operand = true;
                        goto found_match;
                    }

                    // Try swapped
                    bool right_in_root = expressionReferencesTables(&compare->getRightExpression(),
                                                                    root_node.table_aliases);
                    bool left_in_next = expressionReferencesTables(&compare->getLeftExpression(), it->table_aliases);
                    if (right_in_root && left_in_next) {
                        best_pred = std::move(pred);
                        left_is_left_operand = false;
                        goto found_match;
                    }
                }
            }

        found_match:
            VELODB_ASSERT_MSG(it != active_plans.end(), "Cartesian product detected (no join condition found)");

            // Construct Join
            auto& right_node = *it;
            auto* compare = static_cast<ComparisonExpression*>(best_pred.get());

            // Resolve Columns
            auto* root_expr = left_is_left_operand ? &compare->getLeftExpression() : &compare->getRightExpression();
            auto* next_expr = left_is_left_operand ? &compare->getRightExpression() : &compare->getLeftExpression();

            VELODB_ASSERT_MSG(root_expr->getExpressionType() == ExpressionType::COLUMN_REF, "Join key must be column");
            VELODB_ASSERT_MSG(next_expr->getExpressionType() == ExpressionType::COLUMN_REF, "Join key must be column");

            auto* root_col = static_cast<const ColumnRefExpression*>(root_expr);
            auto* next_col = static_cast<const ColumnRefExpression*>(next_expr);

            size_t left_key_idx = root_node.plan->getOutputSchema().getColumnIndex(root_col->getColumnName());
            size_t right_key_idx = right_node.plan->getOutputSchema().getColumnIndex(next_col->getColumnName());

            std::unique_ptr<AbstractPlanNode> join_node;
            if (join_strategy_ == JoinStrategy::HASH_JOIN) {
                // Hash join: find mask columns and build output schema without them
                const auto& left_schema = root_node.plan->getOutputSchema();
                const auto& right_schema = right_node.plan->getOutputSchema();

                // Find mask column indices in left and right schemas
                ssize_t left_mask_idx = -1;
                ssize_t right_mask_idx = -1;
                for (size_t i = 0; i < left_schema.getColumnCount(); ++i) {
                    const auto& [table_alias, col_name] = splitName(left_schema.getColumnInfo(i).getName());
                    if (col_name == "$_mask") {
                        left_mask_idx = static_cast<ssize_t>(i);
                        break;
                    }
                }
                for (size_t i = 0; i < right_schema.getColumnCount(); ++i) {
                    const auto& [table_alias, col_name] = splitName(right_schema.getColumnInfo(i).getName());
                    if (col_name == "$_mask") {
                        right_mask_idx = static_cast<ssize_t>(i);
                        break;
                    }
                }

                // Build output schema without mask columns
                Schema output_schema;
                for (size_t i = 0; i < left_schema.getColumnCount(); ++i) {
                    if (static_cast<ssize_t>(i) != left_mask_idx) {
                        output_schema.addColumnInfo(left_schema.getColumnInfo(i).clone());
                    }
                }
                for (size_t i = 0; i < right_schema.getColumnCount(); ++i) {
                    if (static_cast<ssize_t>(i) != right_mask_idx) {
                        output_schema.addColumnInfo(right_schema.getColumnInfo(i).clone());
                    }
                }

                join_node = std::make_unique<HashJoinPlanNode>(std::move(output_schema),
                                                               std::move(root_node.plan),
                                                               std::move(right_node.plan),
                                                               std::make_pair(left_key_idx, right_key_idx),
                                                               std::make_pair(left_mask_idx, right_mask_idx),
                                                               root_node.source_tables,
                                                               right_node.source_tables);
            } else {
                // Sort-merge join: sort key columns before join
                // Left
                {
                    std::vector<size_t> indices { left_key_idx };
                    std::vector<bool> asc { true };
                    auto sort = std::make_unique<SortPlanNode>(root_node.plan->getOutputSchema().clone(), indices, asc);
                    sort->addChild(std::move(root_node.plan));
                    root_node.plan = std::move(sort);
                }
                // Right
                {
                    std::vector<size_t> indices { right_key_idx };
                    std::vector<bool> asc { true };
                    auto sort = std::make_unique<SortPlanNode>(right_node.plan->getOutputSchema().clone(),
                                                               indices,
                                                               asc);
                    sort->addChild(std::move(right_node.plan));
                    right_node.plan = std::move(sort);
                }

                // Build output_schema after sorting
                Schema output_schema = root_node.plan->getOutputSchema().clone();
                for (const auto& col : right_node.plan->getOutputSchema()) {
                    output_schema.addColumnInfo(col.clone());
                }

                join_node = std::make_unique<SortMergeJoinPlanNode>(std::move(output_schema),
                                                                    std::move(root_node.plan),
                                                                    std::move(right_node.plan),
                                                                    std::make_pair(left_key_idx, right_key_idx),
                                                                    root_node.source_tables,
                                                                    right_node.source_tables);
            }
            root_node.plan = std::move(join_node);
            root_node.table_aliases.insert(root_node.table_aliases.end(),
                                           right_node.table_aliases.begin(),
                                           right_node.table_aliases.end());
            root_node.source_tables.insert(root_node.source_tables.end(),
                                           right_node.source_tables.begin(),
                                           right_node.source_tables.end());

            active_plans.erase(it);
        }

        // 4d. Apply remaining join predicates as filters
        // Some predicates like "c_nationkey = s_nationkey" connect two tables
        // that were both already joined, so they weren't used as join conditions.
        // These must be applied as post-join filters.
        std::unique_ptr<AbstractExpression> remaining_predicate = nullptr;
        for (auto& pred : join_predicates) {
            if (pred) {
                if (!remaining_predicate) {
                    remaining_predicate = std::move(pred);
                } else {
                    remaining_predicate = std::make_unique<BinaryLogicalExpression>(ConnectiveType::AND,
                                                                                    std::move(remaining_predicate),
                                                                                    std::move(pred));
                }
            }
        }
        if (remaining_predicate) {
            // Use GpuFilterPlanNode to apply remaining join predicates on GPU
            auto& in_schema = root_node.plan->getOutputSchema();
            auto filter_plan = std::make_unique<GpuFilterPlanNode>(in_schema.clone(), std::move(remaining_predicate));
            filter_plan->addChild(std::move(root_node.plan));
            root_node.plan = std::move(filter_plan);
        }

        return std::move(root_node.plan);
    }
}

// ... Implement helpers ...

std::unique_ptr<AbstractPlanNode> QueryPlanner::planOrderBy(std::unique_ptr<AbstractPlanNode>&& plan,
                                                            const std::vector<hsql::OrderDescription*>* orders)
{
    const auto& schema = plan->getOutputSchema();
    std::vector<size_t> order_indices;
    std::vector<bool> ascending_flags;
    for (auto* order_desc : *orders) {
        VELODB_ASSERT_MSG(order_desc->expr->type == hsql::kExprColumnRef, "ORDER BY must be column references");
        auto* col_ref = order_desc->expr;
        std::string col_name = col_ref->name;
        if (col_ref->alias != nullptr) {
            col_name = fmt::format("{}.{}", col_ref->alias, col_ref->name);
        } else if (col_ref->table != nullptr) {
            col_name = fmt::format("{}.{}", col_ref->table, col_ref->name);
        }
        auto col_index = schema.getColumnIndex(col_name);
        order_indices.push_back(col_index);
        ascending_flags.push_back(order_desc->type == hsql::kOrderAsc);
    }
    VELODB_ASSERT_MSG(order_indices.size() <= MAX_SORT_COLUMNS,
                      fmt::format("ORDER BY with more than {} columns not supported", MAX_SORT_COLUMNS));
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

void QueryPlanner::collectTableRefs(const hsql::TableRef* table_ref,
                                    std::vector<const hsql::TableRef*>& leaf_tables,
                                    std::vector<std::unique_ptr<AbstractExpression>>& join_conditions)
{
    switch (table_ref->type) {
    case hsql::kTableName: {
        leaf_tables.push_back(table_ref);
        break;
    }
    case hsql::kTableJoin: {
        collectTableRefs(table_ref->join->left, leaf_tables, join_conditions);
        collectTableRefs(table_ref->join->right, leaf_tables, join_conditions);
        if (table_ref->join->condition) {
            // Pass the currently accumulated leaves to parseExpression to avoid recursion
            join_conditions.push_back(parseExpression(leaf_tables, table_ref->join->condition));
        }
        break;
    }
    case hsql::kTableCrossProduct: {
        for (auto* ref : *table_ref->list) {
            collectTableRefs(ref, leaf_tables, join_conditions);
        }
        break;
    }
    default: {
        VELODB_THROW(ExecutionError, "Unsupported table reference type");
    }
    }
}

QueryPlanner::JoinNodeInfo QueryPlanner::planTableLeaf(const hsql::TableRef* table_ref,
                                                       std::vector<std::unique_ptr<AbstractExpression>>& filters)
{
    VELODB_ASSERT_MSG(table_ref->type == hsql::kTableName, "Leaf must be table name");
    std::string table_name = table_ref->name;
    std::string alias = table_ref->alias ? table_ref->alias->name : table_name;

    auto table = catalog_.get().getTable(table_name);
    if (!table) {
        VELODB_THROW(CatalogError, "Table not found: " + table_name);
    }

    // 1. Seq Scan
    auto seq_scan_schema = inferSeqScanSchema(*table, alias);
    // Combine filters
    std::unique_ptr<AbstractExpression> predicate = nullptr;
    if (!filters.empty()) {
        predicate = std::move(filters[0]);
        for (size_t i = 1; i < filters.size(); ++i) {
            predicate = std::make_unique<BinaryLogicalExpression>(ConnectiveType::AND,
                                                                  std::move(predicate),
                                                                  std::move(filters[i]));
        }
    }
    filters.clear();
    auto seq_scan = std::make_unique<SeqScanPlanNode>(*table, seq_scan_schema.clone(), std::move(predicate));

    // Return seq_scan directly - caller decides whether to add FilterCompaction
    return { std::move(seq_scan), { alias }, { &table->get() }, std::move(seq_scan_schema) };
}

bool QueryPlanner::expressionReferencesTables(const AbstractExpression* expr, const std::vector<std::string>& tables)
{
    if (expr->getExpressionType() == ExpressionType::COLUMN_REF) {
        const auto* col_ref = static_cast<const ColumnRefExpression*>(expr);
        const std::string& ref_table = col_ref->getTableName();
        for (const auto& t : tables) {
            if (t == ref_table)
                return true;
        }
        return false;
    }
    if (expr->isUnary()) {
        const auto* un = static_cast<const UnaryExpression*>(expr);
        return expressionReferencesTables(&un->getOperandExpression(), tables);
    } else if (!expr->isLeaf()) {
        const auto* bin = static_cast<const BinaryExpression*>(expr);
        return expressionReferencesTables(&bin->getLeftExpression(), tables)
            && // AND: sub-expression must also be valid in context (usually)
            expressionReferencesTables(&bin->getRightExpression(), tables);
    }
    return true; // Constant
}

Schema QueryPlanner::inferSeqScanSchema(const Table& table, std::string_view table_alias)
{
    VELODB_ASSERT_MSG(!table_alias.empty(), "Table alias cannot be empty for SeqScan schema inference");
    std::vector<ColumnInfo> columns;
    for (const auto& col_info : table.getSchema()) {
        columns.emplace_back(fmt::format("{}.{}", table_alias, col_info.getName()), col_info.getType().cloneUnique());
    }
    columns.emplace_back(fmt::format("{}.$_rowid", table_alias), std::make_unique<BigIntType>());
    columns.emplace_back(fmt::format("{}.$_mask", table_alias), std::make_unique<BooleanType>());
    return Schema(std::move(columns));
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
            columns.emplace_back(fmt::format("col_{}", i), std::move(return_type));
            break;
        default:
            VELODB_THROW(ExecutionError, "Unsupported expression type in SELECT list for schema inference");
        }
    }
    return Schema(std::move(columns));
}

std::vector<std::unique_ptr<AbstractExpression>> QueryPlanner::parseSelectList(
    const hsql::TableRef* table_ref,
    const std::vector<hsql::Expr*>* select_list)
{
    std::vector<std::unique_ptr<AbstractExpression>> expressions;
    VELODB_ASSERT_MSG(select_list != nullptr && !select_list->empty(), "SELECT list cannot be empty");

    if (select_list->size() == 1 && (*select_list)[0]->type == hsql::kExprStar) {
        std::vector<const hsql::TableRef*> leaves;
        std::vector<std::unique_ptr<AbstractExpression>> dummy_conds;
        collectTableRefs(table_ref, leaves, dummy_conds);

        for (const auto* leaf : leaves) {
            const std::string table_name = leaf->name;
            const std::string alias = leaf->alias ? leaf->alias->name : table_name;
            auto table = catalog_.get().getTable(table_name);
            if (!table)
                VELODB_THROW(CatalogError, "Table not found: " + table_name);

            for (const auto& col_info : table->get().getSchema()) {
                // Project as Alias.Col
                expressions.push_back(std::make_unique<ColumnRefExpression>(
                    table_name, // table name (metadata)
                    alias + "." + col_info.getName(), // Column Name in Schema (Qualified)
                    col_info.getType().cloneUnique()));
            }
        }
    } else {
        for (const auto* expr : *select_list) {
            VELODB_ASSERT_MSG(expr->type != hsql::kExprStar, "* can only be used alone in SELECT list");
            expressions.push_back(parseExpression(table_ref, expr));
        }
    }
    return expressions;
}

std::unique_ptr<AbstractExpression> QueryPlanner::parseExpression(
    const std::vector<const hsql::TableRef*>& scope_tables,
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
        result = parseColumnRef(scope_tables, expr);
        if (!result) {
            VELODB_THROW(ExecutionError, fmt::format("Column reference not found: {}", expr->name));
        }
        break;
    case hsql::kExprOperator:
        result = parseOperator(scope_tables, expr);
        break;
    case hsql::kExprStar:
        VELODB_THROW(ExecutionError, "* expression should be handled in parseSelectList, not parseExpression");
    default:
        VELODB_THROW(ExecutionError, "Expression type not implemented");
    }
    VELODB_ASSERT_MSG(g_type_checker.validateExpression(result.get()), g_type_checker.getLastError());
    return result;
}

std::unique_ptr<AbstractExpression> QueryPlanner::parseColumnRef(const std::vector<const hsql::TableRef*>& scope_tables,
                                                                 const hsql::Expr* expr)
{
    std::string col_name = expr->name;
    std::string tbl_name = expr->table ? expr->table : "";

    const hsql::TableRef* match = nullptr;

    if (!tbl_name.empty()) {
        for (const auto* leaf : scope_tables) {
            std::string alias = leaf->alias ? leaf->alias->name : leaf->name;
            if (alias == tbl_name) {
                match = leaf;
                break;
            }
        }
    } else {
        // Implicit
        for (const auto* leaf : scope_tables) {
            // Need to check schema
            std::string alias = leaf->alias ? leaf->alias->name : leaf->name;
            auto table = catalog_.get().getTable(leaf->name);
            if (table && table->get().hasColumn(col_name)) {
                if (match) {
                    VELODB_THROW(ExecutionError, "Ambiguous column reference: " + col_name);
                }
                match = leaf;
            }
        }
    }

    if (!match)
        return nullptr;

    auto table = catalog_.get().getTable(match->name);
    if (!table || !table->get().hasColumn(col_name))
        return nullptr;

    std::string alias = match->alias ? match->alias->name : match->name;
    std::string qualified_name = alias + "." + col_name;

    return std::make_unique<ColumnRefExpression>(match->name,
                                                 qualified_name,
                                                 table->get().getColumnType(col_name).cloneUnique());
}

std::unique_ptr<AbstractExpression> QueryPlanner::parseOperator(const std::vector<const hsql::TableRef*>& scope_tables,
                                                                const hsql::Expr* expr)
{
    // Implement operator planning for all comparison operators
    switch (expr->opType) {
    case hsql::kOpPlus: {
        auto left = parseExpression(scope_tables, expr->expr);
        auto right = parseExpression(scope_tables, expr->expr2);
        auto return_type = g_type_checker.deduceArithmeticType(left->getReturnType(),
                                                               right->getReturnType(),
                                                               ArithmeticType::PLUS);
        return std::make_unique<ArithmeticExpression>(ArithmeticType::PLUS,
                                                      std::move(return_type),
                                                      std::move(left),
                                                      std::move(right));
    }
    case hsql::kOpMinus: {
        auto left = parseExpression(scope_tables, expr->expr);
        auto right = parseExpression(scope_tables, expr->expr2);
        auto return_type = g_type_checker.deduceArithmeticType(left->getReturnType(),
                                                               right->getReturnType(),
                                                               ArithmeticType::MINUS);
        return std::make_unique<ArithmeticExpression>(ArithmeticType::MINUS,
                                                      std::move(return_type),
                                                      std::move(left),
                                                      std::move(right));
    }
    case hsql::kOpAsterisk: {
        auto left = parseExpression(scope_tables, expr->expr);
        auto right = parseExpression(scope_tables, expr->expr2);
        auto return_type = g_type_checker.deduceArithmeticType(left->getReturnType(),
                                                               right->getReturnType(),
                                                               ArithmeticType::MULTIPLY);
        return std::make_unique<ArithmeticExpression>(ArithmeticType::MULTIPLY,
                                                      std::move(return_type),
                                                      std::move(left),
                                                      std::move(right));
    }
    case hsql::kOpSlash: {
        auto left = parseExpression(scope_tables, expr->expr);
        auto right = parseExpression(scope_tables, expr->expr2);
        auto return_type = g_type_checker.deduceArithmeticType(left->getReturnType(),
                                                               right->getReturnType(),
                                                               ArithmeticType::DIVIDE);
        return std::make_unique<ArithmeticExpression>(ArithmeticType::DIVIDE,
                                                      std::move(return_type),
                                                      std::move(left),
                                                      std::move(right));
    }
    case hsql::kOpPercentage: {
        auto left = parseExpression(scope_tables, expr->expr);
        auto right = parseExpression(scope_tables, expr->expr2);
        auto return_type = g_type_checker.deduceArithmeticType(left->getReturnType(),
                                                               right->getReturnType(),
                                                               ArithmeticType::MODULO);
        return std::make_unique<ArithmeticExpression>(ArithmeticType::MODULO,
                                                      std::move(return_type),
                                                      std::move(left),
                                                      std::move(right));
    }
    case hsql::kOpEquals: {
        auto left = parseExpression(scope_tables, expr->expr);
        auto right = parseExpression(scope_tables, expr->expr2);
        return createComparisonOperator(ComparisonType::EQUAL, std::move(left), std::move(right));
    }
    case hsql::kOpNotEquals: {
        auto left = parseExpression(scope_tables, expr->expr);
        auto right = parseExpression(scope_tables, expr->expr2);
        return createComparisonOperator(ComparisonType::NOT_EQUAL, std::move(left), std::move(right));
    }
    case hsql::kOpLess: {
        auto left = parseExpression(scope_tables, expr->expr);
        auto right = parseExpression(scope_tables, expr->expr2);
        return createComparisonOperator(ComparisonType::LESS_THAN, std::move(left), std::move(right));
    }
    case hsql::kOpLessEq: {
        auto left = parseExpression(scope_tables, expr->expr);
        auto right = parseExpression(scope_tables, expr->expr2);
        return createComparisonOperator(ComparisonType::LESS_THAN_OR_EQUAL, std::move(left), std::move(right));
    }
    case hsql::kOpGreater: {
        auto left = parseExpression(scope_tables, expr->expr);
        auto right = parseExpression(scope_tables, expr->expr2);
        return createComparisonOperator(ComparisonType::GREATER_THAN, std::move(left), std::move(right));
    }
    case hsql::kOpGreaterEq: {
        auto left = parseExpression(scope_tables, expr->expr);
        auto right = parseExpression(scope_tables, expr->expr2);
        return createComparisonOperator(ComparisonType::GREATER_THAN_OR_EQUAL, std::move(left), std::move(right));
    }
    case hsql::kOpAnd: {
        auto left = parseExpression(scope_tables, expr->expr);
        auto right = parseExpression(scope_tables, expr->expr2);
        return std::make_unique<BinaryLogicalExpression>(ConnectiveType::AND, std::move(left), std::move(right));
    }
    case hsql::kOpOr: {
        auto left = parseExpression(scope_tables, expr->expr);
        auto right = parseExpression(scope_tables, expr->expr2);
        return std::make_unique<BinaryLogicalExpression>(ConnectiveType::OR, std::move(left), std::move(right));
    }
    case hsql::kOpNot: {
        auto operand = parseExpression(scope_tables, expr->expr);
        return std::make_unique<LogicalNotExpression>(std::move(operand));
    }
    case hsql::kOpBetween: {
        // BETWEEN is: expr BETWEEN low AND high
        // Transform to: (expr >= low) AND (expr <= high)
        VELODB_ASSERT_MSG(expr->exprList != nullptr && expr->exprList->size() == 2,
                          "BETWEEN requires exactly 2 operands");

        auto operand = parseExpression(scope_tables, expr->expr);
        auto low_expr = parseExpression(scope_tables, (*expr->exprList)[0]);
        auto high_expr = parseExpression(scope_tables, (*expr->exprList)[1]);

        // Create (expr >= low)
        auto left_comparison = createComparisonOperator(ComparisonType::GREATER_THAN_OR_EQUAL,
                                                        operand->cloneUnique(),
                                                        std::move(low_expr));
        // Create (expr <= high)
        auto right_comparison = createComparisonOperator(ComparisonType::LESS_THAN_OR_EQUAL,
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
        auto operand = parseExpression(scope_tables, expr->expr);
        std::unique_ptr<AbstractExpression> in_expression = nullptr;
        for (const auto* list_expr : *expr->exprList) {
            auto value_expr = parseExpression(scope_tables, list_expr);
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

std::unique_ptr<AbstractExpression> QueryPlanner::parseExpression(const hsql::TableRef* table_ref,
                                                                  const hsql::Expr* expr)
{
    std::vector<const hsql::TableRef*> leaves;
    std::vector<std::unique_ptr<AbstractExpression>> dummy_conds;
    collectTableRefs(table_ref, leaves, dummy_conds);
    return parseExpression(leaves, expr);
}

std::unique_ptr<AbstractExpression> QueryPlanner::parseColumnRef(const hsql::TableRef* table_ref,
                                                                 const hsql::Expr* expr)
{
    std::vector<const hsql::TableRef*> leaves;
    std::vector<std::unique_ptr<AbstractExpression>> dummy_conds;
    collectTableRefs(table_ref, leaves, dummy_conds);
    return parseColumnRef(leaves, expr);
}

std::unique_ptr<AbstractExpression> QueryPlanner::parseOperator(const hsql::TableRef* table_ref, const hsql::Expr* expr)
{
    std::vector<const hsql::TableRef*> leaves;
    std::vector<std::unique_ptr<AbstractExpression>> dummy_conds;
    collectTableRefs(table_ref, leaves, dummy_conds);
    return parseOperator(leaves, expr);
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

        // Debugging SegFault
        // std::cerr << "Debug: Handling " << left_expr->getTableName() << "." << left_expr->getColumnName() <<
        // std::endl;

        auto table_opt = catalog_.get().getTable(left_expr->getTableName());
        if (!table_opt) {
            VELODB_THROW(ExecutionError, "Table not found: " + left_expr->getTableName());
        }
        auto& table = table_opt->get();

        auto [_, col_name] = splitName(left_expr->getColumnName());

        auto& column = table.getColumn(col_name);
        column.ensureOrdinal(value, type);
        right = std::make_unique<ConstantExpression>(value);
    } else if (left_expression_type == ExpressionType::CONSTANT
               && right_expression_type == ExpressionType::COLUMN_REF) {
        auto* right_expr = static_cast<ColumnRefExpression*>(right.get());
        auto value = static_cast<ConstantExpression*>(left.release())->getValue();

        auto table_opt = catalog_.get().getTable(right_expr->getTableName());
        if (!table_opt) {
            VELODB_THROW(ExecutionError, "Table not found: " + right_expr->getTableName());
        }
        auto& table = table_opt->get();

        auto [_, col_name] = splitName(right_expr->getColumnName());

        auto& column = table.getColumn(col_name);
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

void QueryPlanner::extractTablesFromExpression(const AbstractExpression* expr,
                                               std::unordered_set<std::string_view>& tables)
{
    if (expr->isLeaf()) {
        if (expr->getExpressionType() == ExpressionType::COLUMN_REF) {
            const auto* col_ref = static_cast<const ColumnRefExpression*>(expr);
            const auto& ref_table_name = col_ref->getTableName();
            tables.insert(ref_table_name);
        }
    } else if (expr->isUnary()) {
        const auto& child = static_cast<const UnaryExpression*>(expr)->getOperandExpression();
        extractTablesFromExpression(&child, tables);
    } else {
        // expr must be binary
        const auto& left = static_cast<const BinaryExpression*>(expr)->getLeftExpression();
        const auto& right = static_cast<const BinaryExpression*>(expr)->getRightExpression();
        extractTablesFromExpression(&left, tables);
        extractTablesFromExpression(&right, tables);
    }
}

} // namespace velodb
