#include <gtest/gtest.h>
#include "catalog/catalog.hpp"
#include "catalog/table.hpp"
#include "catalog/schema.hpp"
#include "types/data_type.hpp"

using namespace velodb;

class CatalogTest : public ::testing::Test {
protected:
    void SetUp() override {
        catalog_ = std::make_unique<Catalog>();
    }

    void TearDown() override {
        // Cleanup code if needed
    }

    std::unique_ptr<Catalog> catalog_;
};

TEST_F(CatalogTest, CreateEmptyCatalog) {
    EXPECT_EQ(catalog_->getTableCount(), 0);
    EXPECT_FALSE(catalog_->hasTable("nonexistent"));
}

TEST_F(CatalogTest, CreateAndRetrieveTable) {
    // Create a schema
    auto int_type = std::make_unique<IntegerType>();
    auto varchar_type = std::make_unique<VarcharType>(100);
    
    Column id_col("id", std::move(int_type));
    Column name_col("name", std::move(varchar_type));
    
    auto schema = std::make_unique<Schema>();
    schema->addColumn(std::move(id_col));
    schema->addColumn(std::move(name_col));

    // Create table
    catalog_->createTable("users", std::move(schema));

    EXPECT_EQ(catalog_->getTableCount(), 1);
    EXPECT_TRUE(catalog_->hasTable("users"));

    // Retrieve table
    TableBase* table = catalog_->getTable("users");
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(table->getName(), "users");
    EXPECT_EQ(table->getSchema().getColumnCount(), 2);
}

TEST_F(CatalogTest, MultipleTablesOperations) {
    // Create first table
    auto schema1 = std::make_unique<Schema>();
    auto int_type1 = std::make_unique<IntegerType>();
    Column id_col1("id", std::move(int_type1));
    schema1->addColumn(std::move(id_col1));

    catalog_->createTable("table1", std::move(schema1));

    // Create second table
    auto schema2 = std::make_unique<Schema>();
    auto varchar_type = std::make_unique<VarcharType>(50);
    Column name_col("name", std::move(varchar_type));
    schema2->addColumn(std::move(name_col));

    catalog_->createTable("table2", std::move(schema2));

    EXPECT_EQ(catalog_->getTableCount(), 2);
    EXPECT_TRUE(catalog_->hasTable("table1"));
    EXPECT_TRUE(catalog_->hasTable("table2"));
    EXPECT_FALSE(catalog_->hasTable("table3"));

    // Verify both tables exist and have correct schemas
    TableBase* t1 = catalog_->getTable("table1");
    TableBase* t2 = catalog_->getTable("table2");

    ASSERT_NE(t1, nullptr);
    ASSERT_NE(t2, nullptr);

    EXPECT_EQ(t1->getSchema().getColumnCount(), 1);
    EXPECT_EQ(t2->getSchema().getColumnCount(), 1);

    EXPECT_EQ(t1->getSchema().getColumn(0).getName(), "id");
    EXPECT_EQ(t2->getSchema().getColumn(0).getName(), "name");
}

TEST_F(CatalogTest, DropTable) {
    // Create a table
    auto schema = std::make_unique<Schema>();
    auto int_type = std::make_unique<IntegerType>();
    Column id_col("id", std::move(int_type));
    schema->addColumn(std::move(id_col));

    catalog_->createTable("temp_table", std::move(schema));
    EXPECT_TRUE(catalog_->hasTable("temp_table"));
    EXPECT_EQ(catalog_->getTableCount(), 1);

    // Drop the table
    catalog_->dropTable("temp_table");
    EXPECT_FALSE(catalog_->hasTable("temp_table"));
    EXPECT_EQ(catalog_->getTableCount(), 0);
}

TEST_F(CatalogTest, GetNonexistentTable) {
    TableBase* table = catalog_->getTable("nonexistent");
    EXPECT_EQ(table, nullptr);
}

// TODO: Add more comprehensive catalog tests when additional features are implemented
// - Database/schema management tests
// - Index catalog tests
// - View catalog tests
// - Metadata persistence tests
