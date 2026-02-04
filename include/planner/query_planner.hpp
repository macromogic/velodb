#pragma once

#include "catalog/catalog.hpp"
#include "common/copy_traits.hpp"
#include "expression/expression.hpp"
#include "planner/abstract_plan_node.hpp"
#include "planner/join_strategy.hpp"

#include <SQLParser.h>

#include <memory>

namespace velodb {

} // namespace velodb

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
    explicit QueryPlanner(Catalog& catalog, JoinStrategy join_strategy = JoinStrategy::HASH_JOIN);
    ~QueryPlanner() = default;

    QueryPlanner(QueryPlanner&& other) noexcept = default;
    QueryPlanner& operator=(QueryPlanner&& other) noexcept = default;

    // Join strategy configuration
    void setJoinStrategy(JoinStrategy strategy) { join_strategy_ = strategy; }
    JoinStrategy getJoinStrategy() const { return join_strategy_; }

    // Main planning interface
    std::unique_ptr<AbstractPlanNode> planSelect(const hsql::SelectStatement* select_stmt);

private:
    // Helper methods for planning
    struct JoinNodeInfo {
        std::unique_ptr<AbstractPlanNode> plan;
        std::vector<std::string> table_aliases;
        std::vector<const Table*> source_tables;
        Schema seq_scan_schema; // Original schema from SeqScan (includes mask column)
    };

    std::unique_ptr<AbstractPlanNode> planTables(const hsql::TableRef* table_ref,
                                                 std::unique_ptr<AbstractExpression> where_predicate);

    // Updated helper: returns info about the planned leaf (SeqScan + Filter)
    JoinNodeInfo planTableLeaf(const hsql::TableRef* table_ref,
                               std::vector<std::unique_ptr<AbstractExpression>>& filters);

    // Helpers for recursive table collection
    void collectTableRefs(const hsql::TableRef* table_ref,
                          std::vector<const hsql::TableRef*>& leaf_tables,
                          std::vector<std::unique_ptr<AbstractExpression>>& join_conditions);

    std::unique_ptr<AbstractPlanNode> planOrderBy(std::unique_ptr<AbstractPlanNode>&& plan,
                                                  const std::vector<hsql::OrderDescription*>* orders);
    std::unique_ptr<AbstractPlanNode> planLimitOffset(std::unique_ptr<AbstractPlanNode>&& plan,
                                                      const hsql::LimitDescription* limit_desc);

    Schema inferSeqScanSchema(const Table& table, std::string_view table_alias);
    Schema inferSelectSchema(const std::vector<std::unique_ptr<AbstractExpression>>& expressions);

    std::vector<std::unique_ptr<AbstractExpression>> parseSelectList(const hsql::TableRef* table_ref,
                                                                     const std::vector<hsql::Expr*>* select_list);

    // Expression Parsing (Context-Aware)
    std::unique_ptr<AbstractExpression> parseExpression(const std::vector<const hsql::TableRef*>& scope_tables,
                                                        const hsql::Expr* expr);
    std::unique_ptr<AbstractExpression> parseColumnRef(const std::vector<const hsql::TableRef*>& scope_tables,
                                                       const hsql::Expr* expr);
    std::unique_ptr<AbstractExpression> parseOperator(const std::vector<const hsql::TableRef*>& scope_tables,
                                                      const hsql::Expr* expr);

    // Old helpers (Forwarding to new ones)
    std::unique_ptr<AbstractExpression> parseExpression(const hsql::TableRef* table_ref, const hsql::Expr* expr);
    std::unique_ptr<AbstractExpression> parseColumnRef(const hsql::TableRef* table_ref, const hsql::Expr* expr);
    std::unique_ptr<AbstractExpression> parseOperator(const hsql::TableRef* table_ref, const hsql::Expr* expr);

    std::unique_ptr<AbstractExpression> createComparisonOperator(ComparisonType type,
                                                                 std::unique_ptr<AbstractExpression> left,
                                                                 std::unique_ptr<AbstractExpression> right);

    // Check if expression references any table in the set
    bool expressionReferencesTables(const AbstractExpression* expr, const std::vector<std::string>& tables);
    void extractTablesFromExpression(const AbstractExpression* expr, std::unordered_set<std::string_view>& tables);

    // Helper methods for predicate decomposition
    void extractConjuncts(const AbstractExpression* expr, std::vector<const AbstractExpression*>& conjuncts);

    std::reference_wrapper<Catalog> catalog_; // Wrap to allow move semantics
    JoinStrategy join_strategy_;
};

} // namespace velodb
