#include "catalog/catalog.hpp"
#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include "data/data_type.hpp"

#include <gtest/gtest.h>

using namespace velodb;

class CatalogTest : public ::testing::Test {
protected:
    void SetUp() override { catalog_ = std::make_unique<Catalog>(); }

    void TearDown() override
    {
        // Cleanup code if needed
    }

    std::unique_ptr<Catalog> catalog_;
};

TEST_F(CatalogTest, CreateEmptyCatalog)
{
    EXPECT_EQ(catalog_->getTableCount(), 0);
    EXPECT_FALSE(catalog_->hasTable("nonexistent"));
}

TEST_F(CatalogTest, CreateAndRetrieveTable)
{
    // Create a schema
    auto schema = std::make_unique<Schema>();
    schema->addColumnInfo({ "id", std::make_unique<IntegerType>() });
    schema->addColumnInfo({ "name", std::make_unique<VarcharType>(100) });

    // Create table
    catalog_->createTable("users", std::move(schema));

    EXPECT_EQ(catalog_->getTableCount(), 1);
    EXPECT_TRUE(catalog_->hasTable("users"));

    // Retrieve table
    auto table_result = catalog_->getTable("users");
    ASSERT_TRUE(table_result.has_value());
    auto& table = table_result.value().get();
    EXPECT_EQ(table.getName(), "users");
    EXPECT_EQ(table.getSchema().getColumnCount(), 2);
}

TEST_F(CatalogTest, MultipleTablesOperations)
{
    // Create first table
    auto schema1 = std::make_unique<Schema>();
    schema1->addColumnInfo({ "id", std::make_unique<IntegerType>() });

    catalog_->createTable("table1", std::move(schema1));

    // Create second table
    auto schema2 = std::make_unique<Schema>();
    schema2->addColumnInfo({ "name", std::make_unique<VarcharType>(50) });

    catalog_->createTable("table2", std::move(schema2));

    EXPECT_EQ(catalog_->getTableCount(), 2);
    EXPECT_TRUE(catalog_->hasTable("table1"));
    EXPECT_TRUE(catalog_->hasTable("table2"));
    EXPECT_FALSE(catalog_->hasTable("table3"));

    // Verify both tables exist and have correct schemas
    auto t1_result = catalog_->getTable("table1");
    auto t2_result = catalog_->getTable("table2");

    ASSERT_TRUE(t1_result.has_value());
    ASSERT_TRUE(t2_result.has_value());

    auto& t1 = t1_result.value().get();
    auto& t2 = t2_result.value().get();

    EXPECT_EQ(t1.getSchema().getColumnCount(), 1);
    EXPECT_EQ(t2.getSchema().getColumnCount(), 1);

    EXPECT_EQ(t1.getSchema().getColumnInfo(0).getName(), "id");
    EXPECT_EQ(t2.getSchema().getColumnInfo(0).getName(), "name");
}

TEST_F(CatalogTest, DropTable)
{
    // Create a table
    auto schema = std::make_unique<Schema>();
    schema->addColumnInfo({ "id", std::make_unique<IntegerType>() });

    catalog_->createTable("temp_table", std::move(schema));
    EXPECT_TRUE(catalog_->hasTable("temp_table"));
    EXPECT_EQ(catalog_->getTableCount(), 1);

    // Drop the table
    catalog_->dropTable("temp_table");
    EXPECT_FALSE(catalog_->hasTable("temp_table"));
    EXPECT_EQ(catalog_->getTableCount(), 0);
}

TEST_F(CatalogTest, GetNonexistentTable)
{
    // Attempt to get a table that does not exist
    auto result = catalog_->getTable("nonexistent_table");
    EXPECT_FALSE(result.has_value());
}

// TODO: Add more comprehensive catalog tests when additional features are
// implemented
// - Database/schema management tests
// - Index catalog tests
// - View catalog tests
// - Metadata persistence tests
