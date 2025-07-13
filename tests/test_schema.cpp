#include <gtest/gtest.h>
#include "catalog/schema.hpp"
#include "types/data_type.hpp"

using namespace velodb;

class SchemaTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

TEST_F(SchemaTest, CreateEmptySchema) {
    Schema schema;
    
    EXPECT_EQ(schema.getColumnCount(), 0);
    EXPECT_EQ(schema.getTupleSize(), 0);
}

TEST_F(SchemaTest, AddColumns) {
    Schema schema;
    
    auto int_type = std::make_unique<IntegerType>();
    auto varchar_type = std::make_unique<VarcharType>(255);
    auto bool_type = std::make_unique<BooleanType>();
    
    Column id_col("id", std::move(int_type));
    Column name_col("name", std::move(varchar_type));
    Column active_col("active", std::move(bool_type));

    schema.addColumn(std::move(id_col));
    schema.addColumn(std::move(name_col));
    schema.addColumn(std::move(active_col));

    EXPECT_EQ(schema.getColumnCount(), 3);

    const auto& col0 = schema.getColumn(0);
    EXPECT_EQ(col0.getName(), "id");
    EXPECT_EQ(col0.getType().getTypeId(), DataTypeId::INTEGER);

    const auto& col1 = schema.getColumn(1);
    EXPECT_EQ(col1.getName(), "name");
    EXPECT_EQ(col1.getType().getTypeId(), DataTypeId::VARCHAR);

    const auto& col2 = schema.getColumn(2);
    EXPECT_EQ(col2.getName(), "active");
    EXPECT_EQ(col2.getType().getTypeId(), DataTypeId::BOOLEAN);
}

TEST_F(SchemaTest, GetColumnByIndex) {
    Schema schema;
    
    auto int_type = std::make_unique<IntegerType>();
    auto varchar_type = std::make_unique<VarcharType>(100);
    
    Column id_col("id", std::move(int_type));
    Column name_col("name", std::move(varchar_type));

    schema.addColumn(std::move(id_col));
    schema.addColumn(std::move(name_col));

    const auto& col0 = schema.getColumn(0);
    EXPECT_EQ(col0.getName(), "id");
    EXPECT_EQ(col0.getType().getTypeId(), DataTypeId::INTEGER);
    
    const auto& col1 = schema.getColumn(1);
    EXPECT_EQ(col1.getName(), "name");
    EXPECT_EQ(col1.getType().getTypeId(), DataTypeId::VARCHAR);
}

TEST_F(SchemaTest, GetColumnByName) {
    Schema schema;
    
    auto int_type = std::make_unique<IntegerType>();
    auto double_type = std::make_unique<DoubleType>();
    
    Column user_col("user_id", std::move(int_type));
    Column score_col("score", std::move(double_type));

    schema.addColumn(std::move(user_col));
    schema.addColumn(std::move(score_col));

    const auto& user_col_ref = schema.getColumn("user_id");
    EXPECT_EQ(user_col_ref.getName(), "user_id");
    EXPECT_EQ(user_col_ref.getType().getTypeId(), DataTypeId::INTEGER);
    
    const auto& score_col_ref = schema.getColumn("score");
    EXPECT_EQ(score_col_ref.getName(), "score");
    EXPECT_EQ(score_col_ref.getType().getTypeId(), DataTypeId::DOUBLE);
}

TEST_F(SchemaTest, GetColumnIndex) {
    Schema schema;
    
    auto int_type = std::make_unique<IntegerType>();
    auto varchar_type = std::make_unique<VarcharType>(50);
    auto bool_type = std::make_unique<BooleanType>();
    
    Column id_col("id", std::move(int_type));
    Column name_col("name", std::move(varchar_type));
    Column enabled_col("enabled", std::move(bool_type));

    schema.addColumn(std::move(id_col));
    schema.addColumn(std::move(name_col));
    schema.addColumn(std::move(enabled_col));

    EXPECT_EQ(schema.getColumnIndex("id"), 0);
    EXPECT_EQ(schema.getColumnIndex("name"), 1);
    EXPECT_EQ(schema.getColumnIndex("enabled"), 2);
}

TEST_F(SchemaTest, HasColumn) {
    Schema schema;
    
    auto int_type = std::make_unique<IntegerType>();
    Column test_col("test_col", std::move(int_type));
    schema.addColumn(std::move(test_col));

    EXPECT_TRUE(schema.hasColumn("test_col"));
    EXPECT_FALSE(schema.hasColumn("nonexistent"));
    EXPECT_FALSE(schema.hasColumn(""));
}

TEST_F(SchemaTest, SchemaClone) {
    Schema original;
    
    auto int_type = std::make_unique<IntegerType>();
    auto varchar_type = std::make_unique<VarcharType>(200);
    
    Column id_col("id", std::move(int_type));
    Column desc_col("description", std::move(varchar_type));

    original.addColumn(std::move(id_col));
    original.addColumn(std::move(desc_col));

    auto cloned = original.clone();

    EXPECT_EQ(cloned->getColumnCount(), original.getColumnCount());
    EXPECT_EQ(cloned->getColumn(0).getName(), original.getColumn(0).getName());
    EXPECT_EQ(cloned->getColumn(1).getName(), original.getColumn(1).getName());
    EXPECT_EQ(cloned->getColumn(0).getType().getTypeId(), original.getColumn(0).getType().getTypeId());
    EXPECT_EQ(cloned->getColumn(1).getType().getTypeId(), original.getColumn(1).getType().getTypeId());
}

TEST_F(SchemaTest, SchemaToString) {
    Schema schema;
    
    auto int_type = std::make_unique<IntegerType>();
    auto varchar_type = std::make_unique<VarcharType>(100);
    
    Column id_col("id", std::move(int_type));
    Column name_col("name", std::move(varchar_type));

    schema.addColumn(std::move(id_col));
    schema.addColumn(std::move(name_col));

    std::string schema_str = schema.toString();
    EXPECT_FALSE(schema_str.empty());
    EXPECT_NE(schema_str.find("id"), std::string::npos);
    EXPECT_NE(schema_str.find("name"), std::string::npos);
}

TEST_F(SchemaTest, EmptySchemaOperations) {
    Schema schema;
    
    // Test operations on empty schema
    EXPECT_EQ(schema.getColumnCount(), 0);
    EXPECT_FALSE(schema.hasColumn("any_column"));

    // These should throw or return invalid results
    EXPECT_THROW(schema.getColumn(0), std::exception);
    EXPECT_THROW(schema.getColumn("nonexistent"), std::exception);
}
