#pragma once

#include "planner/abstract_plan_node.hpp"
#include "expression/expression.hpp"
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
class Catalog;
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
    std::unique_ptr<AbstractPlanNode> planTableRef(const hsql::TableRef* table_ref, std::unique_ptr<AbstractExpression> predicate = nullptr);
    std::unique_ptr<AbstractExpression> planExpression(const hsql::Expr* expr);
    static std::unique_ptr<Schema> inferSelectSchema(const hsql::SelectStatement* select_stmt,
        const Schema& input_schema);

    // SELECT list planning
    std::vector<std::unique_ptr<AbstractExpression>> planSelectList(const std::vector<hsql::Expr*>* select_list);
    std::unique_ptr<Schema> inferScanFilterSchema(const Schema& input_schema);
    std::unique_ptr<Schema> inferProjectionSchema(const std::vector<std::unique_ptr<AbstractExpression>>& expressions,
        const Schema& input_schema);

    // Expression planning helpers
    static std::unique_ptr<AbstractExpression> planColumnRef(const hsql::Expr* expr);
    static std::unique_ptr<AbstractExpression> planLiteral(const hsql::Expr* expr);
    std::unique_ptr<AbstractExpression> planOperator(const hsql::Expr* expr);

    Catalog& catalog_;
};

} // namespace velodb
