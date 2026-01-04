#pragma once

#include "catalog/catalog.hpp"
#include "common/copy_traits.hpp"
#include "expression/expression.hpp"
#include "planner/abstract_plan_node.hpp"

#include <SQLParser.h>

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
class QueryPlanner : private NonCopyable {
public:
    explicit QueryPlanner(Catalog& catalog);
    ~QueryPlanner() = default;

    QueryPlanner(QueryPlanner&& other) noexcept = default;
    QueryPlanner& operator=(QueryPlanner&& other) noexcept = default;

    // Main planning interface
    std::unique_ptr<AbstractPlanNode> planSelect(const hsql::SelectStatement* select_stmt);

private:
    // Helper methods for planning
    std::unique_ptr<AbstractPlanNode> planTableRef(const hsql::TableRef* table_ref,
                                                   std::unique_ptr<AbstractExpression> predicate = nullptr);
    std::unique_ptr<AbstractPlanNode> planJoin(const hsql::TableRef* left_ref,
                                               const hsql::TableRef* right_ref,
                                               const hsql::Expr* join_expr,
                                               std::unique_ptr<AbstractExpression> predicate = nullptr);
    std::unique_ptr<AbstractPlanNode> planOrderBy(std::unique_ptr<AbstractPlanNode>&& plan,
                                                  const std::vector<hsql::OrderDescription*>* orders);
    std::unique_ptr<AbstractPlanNode> planLimitOffset(std::unique_ptr<AbstractPlanNode>&& plan,
                                                  const hsql::LimitDescription* limit_desc);

    Schema inferSeqScanSchema(const Table& table);
    Schema inferSelectSchema(const std::vector<std::unique_ptr<AbstractExpression>>& expressions);
    Schema inferJoinSchema(const Table& left_table, const Table& right_table);

    std::vector<std::unique_ptr<AbstractExpression>> parseSelectList(const hsql::TableRef* table_ref,
                                                                    const std::vector<hsql::Expr*>* select_list);
    std::unique_ptr<AbstractExpression> parseExpression(const hsql::TableRef* table_ref, const hsql::Expr* expr);
    std::unique_ptr<AbstractExpression> parseColumnRef(const hsql::TableRef* table_ref, const hsql::Expr* expr);
    std::unique_ptr<AbstractExpression> parseOperator(const hsql::TableRef* table_ref, const hsql::Expr* expr);
    std::unique_ptr<AbstractExpression> createComparisonOperator(ComparisonType type,
                                                               std::unique_ptr<AbstractExpression> left,
                                                               std::unique_ptr<AbstractExpression> right);

    // Helper methods for predicate decomposition
    void extractConjuncts(const AbstractExpression* expr, std::vector<const AbstractExpression*>& conjuncts);
    bool expressionReferencesOnlyTable(const AbstractExpression* expr, const std::string_view table_name);

    std::reference_wrapper<Catalog> catalog_; // Wrap to allow move semantics
};

} // namespace velodb
