#include "../common/test_warmup_utility.hpp"
#include "catalog/schema.hpp"
#include "common/exception.hpp"
#include "data/data_type.hpp"
#include "data/type_checker.hpp"
#include "data/value.hpp"
#include "expression/expression.hpp"

#include <gtest/gtest.h>

using namespace velodb;

class TypeSafetyDemoTest : public test::VeloDBTest {
protected:
    void SetUp() override
    {
        test::VeloDBTest::SetUp();
        // Create a simple schema for testing
        schema_ = std::make_unique<Schema>();
        schema_->addColumnInfo({ "id", std::make_unique<IntegerType>() });
        schema_->addColumnInfo({ "name", std::make_unique<VarcharType>(50) });
        schema_->addColumnInfo({ "price", std::make_unique<DoubleType>() });
        schema_->addColumnInfo({ "active", std::make_unique<BooleanType>() });
    }

    std::unique_ptr<Schema> schema_;
};

TEST_F(TypeSafetyDemoTest, ValidArithmeticExpressions)
{
    // Test valid arithmetic: INTEGER + INTEGER = INTEGER
    auto left_const = std::make_unique<ConstantExpression>(Value::createInteger(10));
    auto right_const = std::make_unique<ConstantExpression>(Value::createInteger(20));

    EXPECT_NO_THROW({
        auto expr = std::make_unique<ArithmeticExpression>(ArithmeticType::PLUS,
                                                           std::move(left_const),
                                                           std::move(right_const));
        EXPECT_EQ(expr->getReturnType().getTypeId(), DataTypeId::INTEGER);
    });

    // Test valid arithmetic: INTEGER + DOUBLE = DOUBLE (type promotion)
    auto int_const = std::make_unique<ConstantExpression>(Value::createInteger(10));
    auto double_const = std::make_unique<ConstantExpression>(Value::createDouble(3.14));

    EXPECT_NO_THROW({
        auto expr = std::make_unique<ArithmeticExpression>(ArithmeticType::PLUS,
                                                           std::move(int_const),
                                                           std::move(double_const));
        EXPECT_EQ(expr->getReturnType().getTypeId(), DataTypeId::DOUBLE);
    });

    // Test division always returns DOUBLE
    auto left_int = std::make_unique<ConstantExpression>(Value::createInteger(100));
    auto right_int = std::make_unique<ConstantExpression>(Value::createInteger(3));

    EXPECT_NO_THROW({
        auto expr = std::make_unique<ArithmeticExpression>(ArithmeticType::DIVIDE,
                                                           std::move(left_int),
                                                           std::move(right_int));
        EXPECT_EQ(expr->getReturnType().getTypeId(), DataTypeId::DOUBLE);
    });
}

TEST_F(TypeSafetyDemoTest, InvalidArithmeticExpressions)
{
    // Test invalid arithmetic: INTEGER + STRING (should throw at construction)
    auto int_const = std::make_unique<ConstantExpression>(Value::createInteger(10));
    auto string_const = std::make_unique<ConstantExpression>(Value::createString("hello"));

    EXPECT_THROW(
        {
            auto expr = std::make_unique<ArithmeticExpression>(ArithmeticType::PLUS,
                                                               std::move(int_const),
                                                               std::move(string_const));
        },
        TypeError);

    // Test invalid modulo: DOUBLE % INTEGER (should throw at construction)
    auto double_const = std::make_unique<ConstantExpression>(Value::createDouble(10.5));
    auto int_const2 = std::make_unique<ConstantExpression>(Value::createInteger(3));

    EXPECT_THROW(
        {
            auto expr = std::make_unique<ArithmeticExpression>(ArithmeticType::MODULO,
                                                               std::move(double_const),
                                                               std::move(int_const2));
        },
        TypeError);
}

TEST_F(TypeSafetyDemoTest, ValidCastExpressions)
{
    // Test valid cast: INTEGER to STRING
    auto int_const = std::make_unique<ConstantExpression>(Value::createInteger(42));
    auto target_type = std::make_unique<VarcharType>(10);

    EXPECT_NO_THROW({
        auto cast_expr = std::make_unique<CastExpression>(std::move(int_const), std::move(target_type));
        EXPECT_EQ(cast_expr->getReturnType().getTypeId(), DataTypeId::VARCHAR);
    });

    // Test valid cast: STRING to INTEGER
    auto string_const = std::make_unique<ConstantExpression>(Value::createString("123"));
    auto int_type = std::make_unique<IntegerType>();

    EXPECT_NO_THROW({
        auto cast_expr = std::make_unique<CastExpression>(std::move(string_const), std::move(int_type));
        EXPECT_EQ(cast_expr->getReturnType().getTypeId(), DataTypeId::INTEGER);
    });
}

TEST_F(TypeSafetyDemoTest, ArithmeticEvaluation)
{
    // Test actual arithmetic evaluation with type safety
    auto left_const = std::make_unique<ConstantExpression>(Value::createInteger(15));
    auto right_const = std::make_unique<ConstantExpression>(Value::createInteger(25));

    auto expr = std::make_unique<ArithmeticExpression>(ArithmeticType::PLUS,
                                                       std::move(left_const),
                                                       std::move(right_const));

    // Evaluate the expression
    Schema dummy_schema;
    ValueTuple dummy_tuple(dummy_schema, {});
    Value result = expr->evaluate(dummy_tuple, dummy_schema);
    EXPECT_FALSE(result.isNull());
    EXPECT_EQ(result.getTypeId(), DataTypeId::INTEGER);
    EXPECT_EQ(result.getInteger(), 40);
}

TEST_F(TypeSafetyDemoTest, CastEvaluation)
{
    // Test actual cast evaluation
    auto int_const = std::make_unique<ConstantExpression>(Value::createInteger(42));
    auto target_type = std::make_unique<VarcharType>(10);

    auto cast_expr = std::make_unique<CastExpression>(std::move(int_const), std::move(target_type));

    // Evaluate the cast
    Schema dummy_schema;
    ValueTuple dummy_tuple(dummy_schema, {});
    Value result = cast_expr->evaluate(dummy_tuple, dummy_schema);
    EXPECT_FALSE(result.isNull());
    EXPECT_EQ(result.getTypeId(), DataTypeId::VARCHAR);
    EXPECT_EQ(result.getString(), "42");
}

TEST_F(TypeSafetyDemoTest, ComplexExpression)
{
    // Test complex expression: (10 + 20) * CAST(3.14 AS INTEGER)
    auto left_const = std::make_unique<ConstantExpression>(Value::createInteger(10));
    auto right_const = std::make_unique<ConstantExpression>(Value::createInteger(20));

    auto add_expr = std::make_unique<ArithmeticExpression>(ArithmeticType::PLUS,
                                                           std::move(left_const),
                                                           std::move(right_const));

    auto double_const = std::make_unique<ConstantExpression>(Value::createDouble(3.14));
    auto int_type = std::make_unique<IntegerType>();

    auto cast_expr = std::make_unique<CastExpression>(std::move(double_const), std::move(int_type));

    auto multiply_expr = std::make_unique<ArithmeticExpression>(ArithmeticType::MULTIPLY,
                                                                std::move(add_expr),
                                                                std::move(cast_expr));

    // The result should be INTEGER (30 * 3 = 90)
    EXPECT_EQ(multiply_expr->getReturnType().getTypeId(), DataTypeId::INTEGER);

    Schema dummy_schema;
    ValueTuple dummy_tuple(dummy_schema, {});
    Value result = multiply_expr->evaluate(dummy_tuple, dummy_schema);
    EXPECT_FALSE(result.isNull());
    EXPECT_EQ(result.getTypeId(), DataTypeId::INTEGER);
    EXPECT_EQ(result.getInteger(), 90); // (10 + 20) * 3 = 90
}
