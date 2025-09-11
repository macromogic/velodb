#include "../common/test_warmup_utility.hpp"
#include "catalog/catalog.hpp"
#include "catalog/column.hpp"
#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include "catalog/table_builder.hpp"
#include "data/data_type.hpp"

#include <gtest/gtest.h>

using namespace velodb;

class CatalogTest : public test::VeloDBTest {
protected:
    void SetUp() override { test::VeloDBTest::SetUp(); }

    void TearDown() override
    {
        test::VeloDBTest::TearDown();
        // Cleanup code if needed
    }

    Catalog catalog_;
};

TEST_F(CatalogTest, CreateEmptyCatalog)
{
    EXPECT_EQ(catalog_.getTableCount(), 0);
    EXPECT_FALSE(catalog_.hasTable("nonexistent"));
}

TEST_F(CatalogTest, CreateAndRetrieveTable)
{
    // Create a schema
    auto schema = Schema();
    schema.addColumnInfo({ "id", std::make_unique<IntegerType>() });
    schema.addColumnInfo({ "name", std::make_unique<VarcharType>(100) });

    // Create table
    auto builder = TableBuilder("users", std::move(schema));
    catalog_.addTable(std::move(builder).build());

    EXPECT_EQ(catalog_.getTableCount(), 1);
    EXPECT_TRUE(catalog_.hasTable("users"));

    // Retrieve table
    auto table_result = catalog_.getTable("users");
    ASSERT_TRUE(table_result.has_value());
    auto& table = table_result.value().get();
    EXPECT_EQ(table.getName(), "users");
    EXPECT_EQ(table.getColumnCount(), 2);
}

TEST_F(CatalogTest, MultipleTablesOperations)
{
    // Create first table
    auto schema1 = Schema();
    schema1.addColumnInfo({ "id", std::make_unique<IntegerType>() });

    auto builder1 = TableBuilder("table1", std::move(schema1));
    catalog_.addTable(std::move(builder1).build());

    // Create second table
    auto schema2 = Schema();
    schema2.addColumnInfo({ "name", std::make_unique<VarcharType>(50) });

    auto builder2 = TableBuilder("table2", std::move(schema2));
    catalog_.addTable(std::move(builder2).build());

    EXPECT_EQ(catalog_.getTableCount(), 2);
    EXPECT_TRUE(catalog_.hasTable("table1"));
    EXPECT_TRUE(catalog_.hasTable("table2"));
    EXPECT_FALSE(catalog_.hasTable("table3"));

    // Verify both tables exist and have correct schemas
    auto t1_result = catalog_.getTable("table1");
    auto t2_result = catalog_.getTable("table2");

    ASSERT_TRUE(t1_result.has_value());
    ASSERT_TRUE(t2_result.has_value());

    auto& t1 = t1_result.value().get();
    auto& t2 = t2_result.value().get();

    EXPECT_EQ(t1.getColumnCount(), 1);
    EXPECT_EQ(t2.getColumnCount(), 1);

    EXPECT_EQ(t1.getColumnName(0), "id");
    EXPECT_EQ(t2.getColumnName(0), "name");
}

TEST_F(CatalogTest, GetNonexistentTable)
{
    // Attempt to get a table that does not exist
    auto result = catalog_.getTable("nonexistent_table");
    EXPECT_FALSE(result.has_value());
}

// TODO: Add more comprehensive catalog tests when additional features are
// implemented
// - Database/schema management tests
// - Index catalog tests
// - Metadata persistence tests
