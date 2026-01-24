#include "../common/test_warmup_utility.hpp"
#include "catalog/table_builder.hpp"
#include "operator/operator.hpp"
#include "planner/plan_visualizer.hpp"
#include "planner/query_planner.hpp"

#include <SQLParser.h>

#include <gtest/gtest.h>

using namespace velodb;

class JoinExecutionTest : public test::VelODBTest {
protected:
    void SetUp() override
    {
        test::VelODBTest::SetUp();
        Schema a;
        a.addColumnInfo({ "id", std::make_unique<IntegerType>() });
        a.addColumnInfo({ "x", std::make_unique<IntegerType>() });
        TableBuilder ta("A", std::move(a));
        for (int i = 0; i < 5; ++i) {
            std::vector<Value> row;
            row.push_back(Value::createInteger(i));
            row.push_back(Value::createInteger(i * 10));
            ta.insertRow(row);
        }
        catalog_.addTable(std::move(ta).build());
        Schema b;
        b.addColumnInfo({ "id", std::make_unique<IntegerType>() });
        b.addColumnInfo({ "y", std::make_unique<IntegerType>() });
        TableBuilder tb("B", std::move(b));
        for (int i = 0; i < 5; ++i) {
            std::vector<Value> row;
            row.push_back(Value::createInteger(i));
            row.push_back(Value::createInteger(100 + i));
            tb.insertRow(row);
        }
        catalog_.addTable(std::move(tb).build());
        planner_ = std::make_unique<QueryPlanner>(catalog_);
    }
    std::unique_ptr<QueryPlanner> planner_;
};

TEST_F(JoinExecutionTest, BasicInnerJoinSelectStar)
{
    std::string sql = "SELECT * FROM A JOIN B ON A.id = B.id";
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    ASSERT_TRUE(result.isValid());
    auto* stmt = result.getStatement(0);
    ASSERT_EQ(stmt->type(), hsql::kStmtSelect);
    auto* select = static_cast<const hsql::SelectStatement*>(stmt);
    auto plan = planner_->planSelect(select);
    fmt::println("Execution Plan:\n{}", PlanVisualizer::visualizeDetailed(plan));
    ExecutionContext ctx(catalog_, task_manager_);
    auto root_op = plan->createOperator(ctx);
    auto batch_result = root_op->next();
    ASSERT_TRUE(batch_result) << batch_result.error();
    auto batch = std::move(batch_result.value());
    batch.to(DataLocation::HOST);
    // Expect 5 joined rows
    EXPECT_EQ(batch.getRowCount(), 5);
    // Expect 4 output columns: A.id, A.x, B.id, B.y (disambiguated as A.id etc.)
    EXPECT_EQ(batch.getColumnCount(), 4);
}
