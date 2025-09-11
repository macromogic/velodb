#pragma once

#include "catalog/catalog.hpp"
#include "expression/expression.hpp"
#include "planner/abstract_plan_node.hpp"

#include <memory>

// Forward declarations for SQL parser
namespace hsql {
struct SQLStatement;
struct SelectStatement;
struct Expr;
struct TableRef;
}

namespace velodb {

// Forward declarations
class Schema;

// Query planner interface
class QueryPlanner {
public:
    explicit QueryPlanner(Catalog& catalog);
    ~QueryPlanner() = default;

    // Delete copy constructor and assignment
    QueryPlanner(const QueryPlanner&) = delete;
    QueryPlanner& operator=(const QueryPlanner&) = delete;

    // Main planning interface
    std::unique_ptr<AbstractPlanNode> planSelect(const hsql::SelectStatement* select_stmt);

private:
    // Helper methods for planning
    std::unique_ptr<AbstractPlanNode> planTableRef(const hsql::TableRef* table_ref,
                                                   std::unique_ptr<AbstractExpression> predicate = nullptr);
    std::unique_ptr<AbstractPlanNode> planJoin(const hsql::TableRef* left_ref,
                                               const hsql::TableRef* right_ref,
                                               const hsql::Expr* join_expr);
    std::unique_ptr<AbstractExpression> planExpression(const hsql::TableRef* table_ref, const hsql::Expr* expr);

    std::vector<std::unique_ptr<AbstractExpression>> planSelectList(const hsql::TableRef* table_ref,
                                                                    const std::vector<hsql::Expr*>* select_list);
    Schema inferSeqScanSchema(const Table& table);
    Schema inferProjectionSchema(const std::vector<std::unique_ptr<AbstractExpression>>& expressions,
                                 const Schema& input_schema);
    Schema inferJoinSchema(const Table& left_table, const Table& right_table);

    // Expression planning helpers
    std::unique_ptr<AbstractExpression> planColumnRef(const hsql::TableRef* table_ref, const hsql::Expr* expr);
    std::unique_ptr<AbstractExpression> planOperator(const hsql::TableRef* table_ref, const hsql::Expr* expr);
    std::unique_ptr<AbstractExpression> planComparisonOperator(ComparisonType type,
                                                               std::unique_ptr<AbstractExpression> left,
                                                               std::unique_ptr<AbstractExpression> right);

    Catalog& catalog_;
};

} // namespace velodb
