#include "../common/test_warmup_utility.hpp"
#include "catalog/column.hpp"
#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include "catalog/table_builder.hpp"
#include "data/data_type.hpp"

#include <gtest/gtest.h>

using namespace velodb;

class TableTest : public test::VelODBTest {
protected:
    void SetUp() override
    {
        test::VelODBTest::SetUp();
        // Create a simple schema for testing
        test_schema_ = Schema();
        test_schema_.addColumnInfo({ "id", std::make_unique<IntegerType>() });
        test_schema_.addColumnInfo({ "name", std::make_unique<VarcharType>(100) });
    }

    void TearDown() override
    {
        test::VelODBTest::TearDown();
        // Cleanup code if needed
    }

    Schema test_schema_;
};

TEST_F(TableTest, CreateTable)
{
    TableBuilder builder("test_table", std::move(test_schema_));
    Table table = std::move(builder).build();

    EXPECT_EQ(table.getName(), "test_table");
    EXPECT_EQ(table.getColumnCount(), 2);
    EXPECT_EQ(table.getColumnName(0), "id");
    EXPECT_EQ(table.getColumnName(1), "name");
    EXPECT_EQ(table.getRowCount(), 0);
    task_manager_.stop(0);
}

TEST_F(TableTest, InsertAndRetrieveTuple)
{
    // Create a fresh schema for this test
    auto schema = Schema();
    schema.addColumnInfo({ "id", std::make_unique<IntegerType>() });
    schema.addColumnInfo({ "name", std::make_unique<VarcharType>(100) });

    TableBuilder builder("test_table", std::move(schema));

    // Create values for insertion
    std::vector<Value> values;
    values.push_back(Value::createInteger(1));
    values.push_back(Value::createString("Alice"));

    // Insert the row using column-based API
    builder.insertRow(std::move(values));
    Table table = std::move(builder).build();

    EXPECT_EQ(table.getRowCount(), 1);

    // Retrieve values using column-based access
    EXPECT_EQ(table.getValue(0, 0).getInteger(), 1);
    EXPECT_EQ(table.getValue(0, 1).getString(), "Alice");
    task_manager_.stop(0);
}

// TODO: Add more comprehensive table tests when table operations are
// implemented
// - Insert tuple tests
// - Delete tuple tests
// - Update tuple tests
// - Scan tests
// - Index tests
