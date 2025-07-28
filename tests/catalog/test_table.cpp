#include <gtest/gtest.h>
#include "catalog/table.hpp"
#include "catalog/schema.hpp"
#include "types/data_type.hpp"

using namespace velodb;

class TableTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a simple schema for testing
        test_schema_ = std::make_unique<Schema>();
        test_schema_->addColumnInfo({ "id", std::make_unique<IntegerType>() });
        test_schema_->addColumnInfo({ "name", std::make_unique<VarcharType>(100) });
    }

    void TearDown() override {
        // Cleanup code if needed
    }

    std::unique_ptr<Schema> test_schema_;
};

TEST_F(TableTest, CreateTable) {
    auto table_info = std::make_unique<TableInfo>("test_table", std::move(test_schema_));
    Table table(std::move(table_info));

    EXPECT_EQ(table.getName(), "test_table");
    EXPECT_EQ(table.getSchema().getColumnCount(), 2);
    EXPECT_EQ(table.getSchema().getColumnInfo(0).getName(), "id");
    EXPECT_EQ(table.getSchema().getColumnInfo(1).getName(), "name");
    EXPECT_FALSE(table.isView());
    EXPECT_EQ(table.getRowCount(), 0);
}

TEST_F(TableTest, TableInfo) {
    auto table_info = std::make_unique<TableInfo>("users", std::move(test_schema_));

    EXPECT_EQ(table_info->getName(), "users");
    EXPECT_EQ(table_info->getColumnCount(), 2);
    EXPECT_EQ(table_info->getSchema().getColumnInfo(0).getName(), "id");
    EXPECT_EQ(table_info->getSchema().getColumnInfo(1).getName(), "name");
}

TEST_F(TableTest, InsertAndRetrieveTuple) {
    // Create a fresh schema for this test
    auto schema = std::make_unique<Schema>();
    schema->addColumnInfo({ "id", std::make_unique<IntegerType>() });
    schema->addColumnInfo({ "name", std::make_unique<VarcharType>(100) });

    auto table_info = std::make_unique<TableInfo>("test_table", std::move(schema));
    Table table(std::move(table_info));
    
    // Create values for insertion
    std::vector<Value> values;
    values.push_back(Value::createInteger(1));
    values.push_back(Value::createString("Alice"));

    // Insert the row using column-based API
    table.insertRow(std::move(values));

    EXPECT_EQ(table.getRowCount(), 1);

    // Retrieve values using column-based access
    EXPECT_EQ(table.getValue(0, 0).getInteger(), 1);
    EXPECT_EQ(table.getValue(0, 1).getString(), "Alice");
}

// TODO: Add more comprehensive table tests when table operations are implemented
// - Insert tuple tests
// - Delete tuple tests  
// - Update tuple tests
// - Scan tests
// - Index tests
