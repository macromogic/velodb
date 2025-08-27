#include "data/data_type.hpp"

#include <gtest/gtest.h>

using namespace velodb;

class DataTypeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Setup code if needed
    }

    void TearDown() override
    {
        // Cleanup code if needed
    }
};

TEST_F(DataTypeTest, IntegerTypeCreation)
{
    auto int_type = std::make_unique<IntegerType>();

    EXPECT_EQ(int_type->getTypeId(), DataTypeId::INTEGER);
    EXPECT_EQ(int_type->size(), sizeof(int32_t));
    EXPECT_TRUE(int_type->isFixedSize());
    EXPECT_TRUE(int_type->isNumeric());
    EXPECT_EQ(int_type->toString(), "INTEGER");
}

TEST_F(DataTypeTest, DoubleTypeCreation)
{
    auto double_type = std::make_unique<DoubleType>();

    EXPECT_EQ(double_type->getTypeId(), DataTypeId::DOUBLE);
    EXPECT_EQ(double_type->size(), sizeof(double));
    EXPECT_TRUE(double_type->isFixedSize());
    EXPECT_TRUE(double_type->isNumeric());
    EXPECT_EQ(double_type->toString(), "DOUBLE");
}

TEST_F(DataTypeTest, VarcharTypeCreation)
{
    auto varchar_type = std::make_unique<VarcharType>(255);

    EXPECT_EQ(varchar_type->getTypeId(), DataTypeId::VARCHAR);
    EXPECT_EQ(varchar_type->size(), 255);
    EXPECT_FALSE(varchar_type->isFixedSize());
    EXPECT_FALSE(varchar_type->isNumeric());
    EXPECT_EQ(varchar_type->toString(), "VARCHAR(255)");
}

TEST_F(DataTypeTest, BooleanTypeCreation)
{
    auto bool_type = std::make_unique<BooleanType>();

    EXPECT_EQ(bool_type->getTypeId(), DataTypeId::BOOLEAN);
    EXPECT_EQ(bool_type->size(), sizeof(bool));
    EXPECT_TRUE(bool_type->isFixedSize());
    EXPECT_FALSE(bool_type->isNumeric());
    EXPECT_EQ(bool_type->toString(), "BOOLEAN");
}

TEST_F(DataTypeTest, BigIntTypeCreation)
{
    auto bigint_type = std::make_unique<BigIntType>();

    EXPECT_EQ(bigint_type->getTypeId(), DataTypeId::BIGINT);
    EXPECT_EQ(bigint_type->size(), sizeof(int64_t));
    EXPECT_TRUE(bigint_type->isFixedSize());
    EXPECT_TRUE(bigint_type->isNumeric());
    EXPECT_EQ(bigint_type->toString(), "BIGINT");
}

TEST_F(DataTypeTest, CreateTypeFactory)
{
    auto int_type = DataType::createType(DataTypeId::INTEGER);
    EXPECT_EQ(int_type->getTypeId(), DataTypeId::INTEGER);

    auto double_type = DataType::createType(DataTypeId::DOUBLE);
    EXPECT_EQ(double_type->getTypeId(), DataTypeId::DOUBLE);

    auto varchar_type = DataType::createType(DataTypeId::VARCHAR, 100);
    EXPECT_EQ(varchar_type->getTypeId(), DataTypeId::VARCHAR);
    EXPECT_EQ(varchar_type->size(), 100);

    auto bool_type = DataType::createType(DataTypeId::BOOLEAN);
    EXPECT_EQ(bool_type->getTypeId(), DataTypeId::BOOLEAN);
}

TEST_F(DataTypeTest, CreateTypeWithSize)
{
    auto int_type = DataType::createType(DataTypeId::INTEGER, sizeof(int32_t));
    EXPECT_EQ(int_type->getTypeId(), DataTypeId::INTEGER);
    EXPECT_EQ(int_type->size(), sizeof(int32_t));
}

TEST_F(DataTypeTest, TypeComparison)
{
    auto int_type1 = DataType::createType(DataTypeId::INTEGER);
    auto int_type2 = DataType::createType(DataTypeId::INTEGER);
    auto double_type = DataType::createType(DataTypeId::DOUBLE);

    EXPECT_EQ(int_type1->getTypeId(), int_type2->getTypeId());
    EXPECT_NE(int_type1->getTypeId(), double_type->getTypeId());
}
