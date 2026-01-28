#include "../common/test_warmup_utility.hpp"
#include "catalog/table_builder.hpp"
#include "operator/operator.hpp"
#include "planner/plan_visualizer.hpp"
#include "planner/query_planner.hpp"

#include <SQLParser.h>

#include <gtest/gtest.h>

using namespace velodb;

class MultiJoinTest : public test::VelODBTest {
protected:
    void SetUp() override
    {
        test::VelODBTest::SetUp();

        // Table A: ids {1, 2, 3}
        {
            Schema s;
            s.addColumnInfo({ "id", std::make_unique<IntegerType>() });
            s.addColumnInfo({ "val_a", std::make_unique<IntegerType>() });
            TableBuilder t("A", std::move(s));
            t.insertRow({ Value::createInteger(1), Value::createInteger(10) });
            t.insertRow({ Value::createInteger(2), Value::createInteger(20) });
            t.insertRow({ Value::createInteger(3), Value::createInteger(30) });
            catalog_.addTable(std::move(t).build());
        }

        // Table B: ids {1, 2, 4}
        {
            Schema s;
            s.addColumnInfo({ "id", std::make_unique<IntegerType>() });
            s.addColumnInfo({ "val_b", std::make_unique<IntegerType>() });
            TableBuilder t("B", std::move(s));
            t.insertRow({ Value::createInteger(1), Value::createInteger(100) });
            t.insertRow({ Value::createInteger(2), Value::createInteger(200) });
            t.insertRow({ Value::createInteger(4), Value::createInteger(400) });
            catalog_.addTable(std::move(t).build());
        }

        // Table C: ids {1, 3, 5}
        {
            Schema s;
            s.addColumnInfo({ "id", std::make_unique<IntegerType>() });
            s.addColumnInfo({ "val_c", std::make_unique<IntegerType>() });
            TableBuilder t("C", std::move(s));
            t.insertRow({ Value::createInteger(1), Value::createInteger(1000) });
            t.insertRow({ Value::createInteger(3), Value::createInteger(3000) });
            t.insertRow({ Value::createInteger(5), Value::createInteger(5000) });
            catalog_.addTable(std::move(t).build());
        }

        planner_ = std::make_unique<QueryPlanner>(catalog_);
    }
    std::unique_ptr<QueryPlanner> planner_;
};

TEST_F(MultiJoinTest, ThreeTableJoinChain)
{
    // A(1,2,3) JOIN B(1,2,4) -> Matches {1, 2}
    // {1, 2} JOIN C(1,3,5) -> Matches {1}
    // Expected Result: 1 row (id=1)

    std::string sql = "SELECT * FROM A JOIN B ON A.id = B.id JOIN C ON B.id = C.id";
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    ASSERT_TRUE(result.isValid());

    const auto* stmt = static_cast<const hsql::SelectStatement*>(result.getStatement(0));
    auto plan = planner_->planSelect(stmt);
    fmt::println("Execution Plan:\n{}", PlanVisualizer::visualizeDetailed(plan));
    ExecutionContext context(catalog_, task_manager_);
    auto op = plan->createOperator(context);

    auto batchResult = op->next();
    ASSERT_TRUE(batchResult) << (batchResult ? "" : batchResult.error());

    auto batch = std::move(batchResult.value());
    ASSERT_EQ(batch.getRowCount(), 1);

    // Verify next calls return empty or handle EOF
    auto endResult = op->next();
    if (endResult) {
        EXPECT_EQ(endResult.value().getRowCount(), 0);
    }
}

TEST_F(MultiJoinTest, ThreeTableImplicitJoin)
{
    // Same logic but with implicit syntax
    // WHERE clause separation logic testing
    std::string sql = "SELECT * FROM A, B, C WHERE A.id = B.id AND B.id = C.id";
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    ASSERT_TRUE(result.isValid());

    const auto* stmt = static_cast<const hsql::SelectStatement*>(result.getStatement(0));
    auto plan = planner_->planSelect(stmt);
    fmt::println("Execution Plan:\n{}", PlanVisualizer::visualizeDetailed(plan));
    ExecutionContext context(catalog_, task_manager_);
    auto op = plan->createOperator(context);

    auto batchResult = op->next();
    ASSERT_TRUE(batchResult) << (batchResult ? "" : batchResult.error());

    auto batch = std::move(batchResult.value());
    ASSERT_EQ(batch.getRowCount(), 1);

    auto endResult = op->next();
    if (endResult) {
        EXPECT_EQ(endResult.value().getRowCount(), 0);
    }
}
