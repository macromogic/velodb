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
}

TEST_F(SchemaTest, AddColumns) {
    Schema schema;

    schema.addColumnInfo({ "id", std::make_unique<IntegerType>() });
    schema.addColumnInfo({ "name", std::make_unique<VarcharType>(255) });
    schema.addColumnInfo({ "active", std::make_unique<BooleanType>() });

    EXPECT_EQ(schema.getColumnCount(), 3);

    const auto& col0 = schema.getColumnInfo(0);
    EXPECT_EQ(col0.getName(), "id");
    EXPECT_EQ(col0.getType().getTypeId(), DataTypeId::INTEGER);

    const auto& col1 = schema.getColumnInfo(1);
    EXPECT_EQ(col1.getName(), "name");
    EXPECT_EQ(col1.getType().getTypeId(), DataTypeId::VARCHAR);

    const auto& col2 = schema.getColumnInfo(2);
    EXPECT_EQ(col2.getName(), "active");
    EXPECT_EQ(col2.getType().getTypeId(), DataTypeId::BOOLEAN);
}

TEST_F(SchemaTest, GetColumnByIndex) {
    Schema schema;

    schema.addColumnInfo({ "id", std::make_unique<IntegerType>() });
    schema.addColumnInfo({ "name", std::make_unique<VarcharType>(255) });

    const auto& col0 = schema.getColumnInfo(0);
    EXPECT_EQ(col0.getName(), "id");
    EXPECT_EQ(col0.getType().getTypeId(), DataTypeId::INTEGER);

    const auto& col1 = schema.getColumnInfo(1);
    EXPECT_EQ(col1.getName(), "name");
    EXPECT_EQ(col1.getType().getTypeId(), DataTypeId::VARCHAR);
}

TEST_F(SchemaTest, GetColumnByName) {
    Schema schema;

    schema.addColumnInfo({ "user_id", std::make_unique<IntegerType>() });
    schema.addColumnInfo({ "score", std::make_unique<DoubleType>() });

    const auto& user_col_ref = schema.getColumnInfo("user_id");
    EXPECT_EQ(user_col_ref.getName(), "user_id");
    EXPECT_EQ(user_col_ref.getType().getTypeId(), DataTypeId::INTEGER);

    const auto& score_col_ref = schema.getColumnInfo("score");
    EXPECT_EQ(score_col_ref.getName(), "score");
    EXPECT_EQ(score_col_ref.getType().getTypeId(), DataTypeId::DOUBLE);
}

TEST_F(SchemaTest, GetColumnIndex) {
    Schema schema;

    schema.addColumnInfo({ "id", std::make_unique<IntegerType>() });
    schema.addColumnInfo({ "name", std::make_unique<VarcharType>(50) });
    schema.addColumnInfo({ "enabled", std::make_unique<BooleanType>() });

    EXPECT_EQ(schema.getColumnIndex("id"), 0);
    EXPECT_EQ(schema.getColumnIndex("name"), 1);
    EXPECT_EQ(schema.getColumnIndex("enabled"), 2);
}

TEST_F(SchemaTest, HasColumn) {
    Schema schema;

    schema.addColumnInfo({ "test_col", std::make_unique<IntegerType>() });

    EXPECT_TRUE(schema.hasColumn("test_col"));
    EXPECT_FALSE(schema.hasColumn("nonexistent"));
    EXPECT_FALSE(schema.hasColumn(""));
}

TEST_F(SchemaTest, SchemaClone) {
    Schema original;

    original.addColumnInfo({ "id", std::make_unique<IntegerType>() });
    original.addColumnInfo({ "description", std::make_unique<VarcharType>(200) });

    auto cloned = original.cloneUnique();

    EXPECT_EQ(cloned->getColumnCount(), original.getColumnCount());
    EXPECT_EQ(cloned->getColumnInfo(0).getName(), original.getColumnInfo(0).getName());
    EXPECT_EQ(cloned->getColumnInfo(1).getName(), original.getColumnInfo(1).getName());
    EXPECT_EQ(cloned->getColumnInfo(0).getType().getTypeId(), original.getColumnInfo(0).getType().getTypeId());
    EXPECT_EQ(cloned->getColumnInfo(1).getType().getTypeId(), original.getColumnInfo(1).getType().getTypeId());
}

TEST_F(SchemaTest, SchemaToString) {
    Schema schema;

    schema.addColumnInfo({ "id", std::make_unique<IntegerType>() });
    schema.addColumnInfo({ "name", std::make_unique<VarcharType>(100) });

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
    EXPECT_THROW(schema.getColumnInfo(0), std::exception);
    EXPECT_THROW(schema.getColumnInfo("nonexistent"), std::exception);
}
