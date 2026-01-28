#include "../common/test_warmup_utility.hpp"
#include "catalog/catalog.hpp"
#include "catalog/table_builder.hpp"
#include "planner/abstract_plan_node.hpp"
#include "planner/query_planner.hpp"

#include <SQLParser.h>

#include <gtest/gtest.h>

using namespace velodb;

class JoinPlannerTest : public test::VelODBTest {
protected:
    void SetUp() override
    {
        test::VelODBTest::SetUp();
        // left table
        Schema left_schema;
        left_schema.addColumnInfo({ "id", std::make_unique<IntegerType>() });
        left_schema.addColumnInfo({ "val", std::make_unique<IntegerType>() });
        TableBuilder lb("left_t", std::move(left_schema));
        for (int i = 0; i < 3; ++i) {
            std::vector<Value> row;
            row.push_back(Value::createInteger(i));
            row.push_back(Value::createInteger(i * 10));
            lb.insertRow(row);
        }
        catalog_.addTable(std::move(lb).build());
        // right table
        Schema right_schema;
        right_schema.addColumnInfo({ "id", std::make_unique<IntegerType>() });
        right_schema.addColumnInfo({ "attr", std::make_unique<IntegerType>() });
        TableBuilder rb("right_t", std::move(right_schema));
        for (int i = 0; i < 3; ++i) {
            std::vector<Value> row;
            row.push_back(Value::createInteger(i));
            row.push_back(Value::createInteger(100 + i));
            rb.insertRow(row);
        }
        catalog_.addTable(std::move(rb).build());
        planner_ = std::make_unique<QueryPlanner>(catalog_);
    }
    Catalog catalog_;
    std::unique_ptr<QueryPlanner> planner_;
};

TEST_F(JoinPlannerTest, SelectStarJoinPlanContainsMaterialization)
{
    std::string sql = "SELECT * FROM left_t INNER JOIN right_t ON left_t.id = right_t.id";
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    ASSERT_TRUE(result.isValid());
    ASSERT_EQ(result.size(), 1);
    auto* stmt = result.getStatement(0);
    ASSERT_EQ(stmt->type(), hsql::kStmtSelect);
    auto* select = static_cast<const hsql::SelectStatement*>(stmt);
    auto plan = planner_->planSelect(select);
    // Top should be Materialization (PlanType::PROJECTION reused) or Limit/Order wrapping it
    // Traverse until reach merge join
    std::function<const AbstractPlanNode*(const AbstractPlanNode*)> find_join =
        [&](const AbstractPlanNode* n) -> const AbstractPlanNode* {
        if (!n)
            return nullptr;
        if (n->getPlanType() == PlanType::SORT_MERGE_JOIN)
            return n;
        for (auto& c : n->getChildren()) {
            auto r = find_join(c.get());
            if (r)
                return r;
        }
        return nullptr;
    };
    auto join_node = find_join(plan.get());
    ASSERT_NE(join_node, nullptr);

    // Ensure a materialization node exists above join (its direct parent or ancestor with disambiguated schema)
    bool found_materialization = false;
    std::function<bool(const AbstractPlanNode*)> contains_join = [&](const AbstractPlanNode* n) {
        if (!n) {
            return false;
        }
        if (n == join_node) {
            return true;
        }
        for (auto& c : n->getChildren()) {
            if (contains_join(c.get())) {
                return true;
            }
        }
        return false;
    };
    std::function<void(const AbstractPlanNode*)> search_mat = [&](const AbstractPlanNode* n) {
        if (!n) {
            return;
        }
        if (n->getPlanType() == PlanType::MATERIALIZATION) {
            if (contains_join(n)) {
                found_materialization = true;
            }
        }
        for (auto& c : n->getChildren()) {
            search_mat(c.get());
        }
    };
    search_mat(plan.get());
    EXPECT_TRUE(found_materialization);
}
