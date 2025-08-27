#include "data/data_type.hpp"
#include "data/type_checker.hpp"
#include "data/value.hpp"
#include "expression/expression.hpp"

#include <gtest/gtest.h>

using namespace velodb;

class TypeCheckerTest : public ::testing::Test {
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

TEST_F(TypeCheckerTest, ArithmeticTypeDeduction)
{
    // Test integer + integer = integer
    auto int_type = std::make_unique<IntegerType>();
    auto result = g_type_checker.deduceArithmeticType(*int_type, *int_type, ArithmeticType::PLUS);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getTypeId(), DataTypeId::INTEGER);

    // Test integer + bigint = bigint
    auto bigint_type = std::make_unique<BigIntType>();
    result = g_type_checker.deduceArithmeticType(*int_type, *bigint_type, ArithmeticType::PLUS);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getTypeId(), DataTypeId::BIGINT);

    // Test integer + double = double
    auto double_type = std::make_unique<DoubleType>();
    result = g_type_checker.deduceArithmeticType(*int_type, *double_type, ArithmeticType::PLUS);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getTypeId(), DataTypeId::DOUBLE);

    // Test division always returns double
    result = g_type_checker.deduceArithmeticType(*int_type, *int_type, ArithmeticType::DIVIDE);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getTypeId(), DataTypeId::DOUBLE);
}

TEST_F(TypeCheckerTest, ArithmeticTypeValidation)
{
    auto int_type = std::make_unique<IntegerType>();
    auto varchar_type = std::make_unique<VarcharType>(50);

    // Test invalid arithmetic with string
    auto result = g_type_checker.deduceArithmeticType(*int_type, *varchar_type, ArithmeticType::PLUS);
    EXPECT_EQ(result, nullptr);
    EXPECT_FALSE(g_type_checker.getLastError().empty());
}

TEST_F(TypeCheckerTest, ModuloTypeValidation)
{
    auto int_type = std::make_unique<IntegerType>();
    auto double_type = std::make_unique<DoubleType>();

    // Test modulo with integer types (should work)
    auto result = g_type_checker.deduceArithmeticType(*int_type, *int_type, ArithmeticType::MODULO);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getTypeId(), DataTypeId::INTEGER);

    // Test modulo with double (should fail)
    result = g_type_checker.deduceArithmeticType(*int_type, *double_type, ArithmeticType::MODULO);
    EXPECT_EQ(result, nullptr);
    EXPECT_FALSE(g_type_checker.getLastError().empty());
}

TEST_F(TypeCheckerTest, ComparisonValidation)
{
    auto int_type = std::make_unique<IntegerType>();
    auto double_type = std::make_unique<DoubleType>();
    auto varchar_type = std::make_unique<VarcharType>(50);

    // Test numeric comparison (should work)
    EXPECT_TRUE(g_type_checker.validateComparison(*int_type, *double_type, ComparisonType::EQUAL));

    // Test string comparison (should work)
    EXPECT_TRUE(g_type_checker.validateComparison(*varchar_type, *varchar_type, ComparisonType::EQUAL));

    // Test LIKE with strings (should work)
    EXPECT_TRUE(g_type_checker.validateComparison(*varchar_type, *varchar_type, ComparisonType::LIKE));

    // Test LIKE with non-strings (should fail)
    EXPECT_FALSE(g_type_checker.validateComparison(*int_type, *varchar_type, ComparisonType::LIKE));
}

TEST_F(TypeCheckerTest, TypeConversion)
{
    auto int_type = std::make_unique<IntegerType>();
    auto bigint_type = std::make_unique<BigIntType>();
    auto double_type = std::make_unique<DoubleType>();
    auto varchar_type = std::make_unique<VarcharType>(50);

    // Test implicit conversions (widening)
    EXPECT_EQ(g_type_checker.canConvert(*int_type, *bigint_type), ConversionResult::VALID);
    EXPECT_EQ(g_type_checker.canConvert(*int_type, *double_type), ConversionResult::VALID);

    // Test narrowing conversions (explicit only)
    EXPECT_EQ(g_type_checker.canConvert(*bigint_type, *int_type), ConversionResult::VALID_WITH_LOSS);

    // Test string to numeric (runtime check needed)
    EXPECT_EQ(g_type_checker.canConvert(*varchar_type, *int_type), ConversionResult::RUNTIME_CHECK);

    // Test invalid conversions
    auto bool_type = std::make_unique<BooleanType>();
    EXPECT_EQ(g_type_checker.canConvert(*bool_type, *varchar_type), ConversionResult::VALID);
}

TEST_F(TypeCheckerTest, CastValidation)
{
    auto int_type = std::make_unique<IntegerType>();
    auto varchar_type = std::make_unique<VarcharType>(50);
    auto bool_type = std::make_unique<BooleanType>();

    // Test valid casts
    EXPECT_TRUE(g_type_checker.validateCast(*int_type, *varchar_type));
    EXPECT_TRUE(g_type_checker.validateCast(*varchar_type, *int_type));
    EXPECT_TRUE(g_type_checker.validateCast(*bool_type, *int_type));

    // All explicit conversions should be valid for cast
    auto double_type = std::make_unique<DoubleType>();
    EXPECT_TRUE(g_type_checker.validateCast(*double_type, *int_type));
}

TEST_F(TypeCheckerTest, TypePromotion)
{
    auto int_type = std::make_unique<IntegerType>();
    auto bigint_type = std::make_unique<BigIntType>();
    auto double_type = std::make_unique<DoubleType>();

    // Test promotion to higher rank type
    auto result = g_type_checker.promoteTypes(*int_type, *bigint_type);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getTypeId(), DataTypeId::BIGINT);

    result = g_type_checker.promoteTypes(*bigint_type, *double_type);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getTypeId(), DataTypeId::DOUBLE);

    // Test same types
    result = g_type_checker.promoteTypes(*int_type, *int_type);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getTypeId(), DataTypeId::INTEGER);
}

TEST_F(TypeCheckerTest, TypeUtilities)
{
    auto int_type = std::make_unique<IntegerType>();
    auto varchar_type = std::make_unique<VarcharType>(50);
    auto bool_type = std::make_unique<BooleanType>();

    // Test type classification
    EXPECT_TRUE(g_type_checker.isNumericType(*int_type));
    EXPECT_FALSE(g_type_checker.isNumericType(*varchar_type));
    EXPECT_FALSE(g_type_checker.isNumericType(*bool_type));

    EXPECT_TRUE(g_type_checker.isStringType(*varchar_type));
    EXPECT_FALSE(g_type_checker.isStringType(*int_type));

    // Test type ranks
    EXPECT_GT(g_type_checker.getTypeRank(*int_type), g_type_checker.getTypeRank(*bool_type));
    EXPECT_GT(g_type_checker.getTypeRank(*varchar_type), g_type_checker.getTypeRank(*int_type));
}
