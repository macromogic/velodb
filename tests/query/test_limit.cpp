#include "../common/test_warmup_utility.hpp"
#include "catalog/execution_context.hpp"
#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include "catalog/table_builder.hpp"
#include "data/data_type.hpp"
#include "execution/execution_engine.hpp"

#include <SQLParser.h>

#include <gtest/gtest.h>

using namespace velodb;

class LimitOperatorTest : public test::VeloDBTest {
public:
    LimitOperatorTest()
        : catalog_()
        , engine_(catalog_)
    {
    }

protected:
    void SetUp() override
    {
        test::VeloDBTest::SetUp();

        // Create a test table with more sample data for comprehensive testing
        auto schema = Schema();
        schema.addColumnInfo({ "id", std::make_unique<IntegerType>() });
        schema.addColumnInfo({ "name", std::make_unique<VarcharType>(100) });
        schema.addColumnInfo({ "age", std::make_unique<IntegerType>() });

        auto builder = TableBuilder("students", std::move(schema));

        // Insert 10 test records
        for (int i = 1; i <= 10; i++) {
            std::vector<Value> values;
            values.push_back(Value::createInteger(i));
            values.push_back(Value::createString("User" + std::to_string(i)));
            values.push_back(Value::createInteger(20 + i));
            builder.insertRow(values);
        }

        catalog_.addTable(std::move(builder).build());
    }

    void TearDown() override { test::VeloDBTest::TearDown(); }

    Catalog catalog_;
    ExecutionEngine engine_;
};

TEST_F(LimitOperatorTest, LimitExecution)
{
    std::string sql = "SELECT id, name FROM students LIMIT 3 OFFSET 1";

    auto result = engine_.executeQuery(sql);
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();
    auto& view = result.value();

    // Should return exactly 3 rows (LIMIT 3)
    EXPECT_EQ(view.getRowCount(), 3);
}
