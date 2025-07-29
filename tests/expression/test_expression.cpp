#include "expression/expression.hpp"
#include "types/data_type.hpp"
#include "types/value.hpp"
#include <gtest/gtest.h>

using namespace velodb;

class ExpressionTest : public ::testing::Test {
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

TEST_F(ExpressionTest, ConstantExpression)
{
    Value int_val = Value::createInteger(42);
    ConstantExpression const_expr(int_val);

    EXPECT_EQ(const_expr.getReturnType().getTypeId(), DataTypeId::INTEGER);
    EXPECT_EQ(const_expr.toString(), "42");

    // TODO: Test Evaluate when execution context is available
}

TEST_F(ExpressionTest, ColumnRefExpression)
{
    auto int_type = std::make_unique<IntegerType>();
    ColumnRefExpression col_expr("user_id", std::move(int_type));

    EXPECT_EQ(col_expr.getReturnType().getTypeId(), DataTypeId::INTEGER);
    EXPECT_EQ(col_expr.getColumnName(), "user_id");
    EXPECT_EQ(col_expr.toString(), "user_id");
}

TEST_F(ExpressionTest, ComparisonExpression)
{
    Value left_val = Value::createInteger(10);
    Value right_val = Value::createInteger(20);

    auto left_expr = std::make_unique<ConstantExpression>(left_val);
    auto right_expr = std::make_unique<ConstantExpression>(right_val);

    ComparisonExpression comp_expr(ComparisonType::EQUAL, std::move(left_expr), std::move(right_expr));

    EXPECT_EQ(comp_expr.getReturnType().getTypeId(), DataTypeId::BOOLEAN);
    EXPECT_EQ(comp_expr.getComparisonType(), ComparisonType::EQUAL);

    std::string expr_str = comp_expr.toString();
    EXPECT_FALSE(expr_str.empty());
    EXPECT_NE(expr_str.find("="), std::string::npos);
}

TEST_F(ExpressionTest, ArithmeticExpression)
{
    Value left_val = Value::createInteger(5);
    Value right_val = Value::createInteger(3);

    auto left_expr = std::make_unique<ConstantExpression>(left_val);
    auto right_expr = std::make_unique<ConstantExpression>(right_val);

    ArithmeticExpression arith_expr(ArithmeticType::PLUS, std::move(left_expr), std::move(right_expr));

    EXPECT_EQ(arith_expr.getReturnType().getTypeId(), DataTypeId::INTEGER);
    EXPECT_EQ(arith_expr.getArithmeticType(), ArithmeticType::PLUS);

    std::string expr_str = arith_expr.toString();
    EXPECT_FALSE(expr_str.empty());
    EXPECT_NE(expr_str.find("+"), std::string::npos);
}

TEST_F(ExpressionTest, BinaryLogicalExpression)
{
    Value left_val = Value::createBoolean(true);
    Value right_val = Value::createBoolean(false);

    auto left_expr = std::make_unique<ConstantExpression>(left_val);
    auto right_expr = std::make_unique<ConstantExpression>(right_val);

    BinaryLogicalExpression and_expr(ConnectiveType::AND, std::move(left_expr), std::move(right_expr));

    EXPECT_EQ(and_expr.getReturnType().getTypeId(), DataTypeId::BOOLEAN);
    EXPECT_EQ(and_expr.getConjunctionType(), ConnectiveType::AND);

    std::string expr_str = and_expr.toString();
    EXPECT_FALSE(expr_str.empty());
    EXPECT_NE(expr_str.find("AND"), std::string::npos);
}

TEST_F(ExpressionTest, NestedExpressions)
{
    // Create (5 + 3) > 7
    Value val5 = Value::createInteger(5);
    Value val3 = Value::createInteger(3);
    Value val7 = Value::createInteger(7);

    auto expr5 = std::make_unique<ConstantExpression>(val5);
    auto expr3 = std::make_unique<ConstantExpression>(val3);
    auto expr7 = std::make_unique<ConstantExpression>(val7);

    auto add_expr = std::make_unique<ArithmeticExpression>(
        ArithmeticType::PLUS, std::move(expr5), std::move(expr3));

    ComparisonExpression comp_expr(
        ComparisonType::GREATER_THAN, std::move(add_expr), std::move(expr7));

    EXPECT_EQ(comp_expr.getReturnType().getTypeId(), DataTypeId::BOOLEAN);

    std::string expr_str = comp_expr.toString();
    EXPECT_FALSE(expr_str.empty());
    // Should contain both + and >
    EXPECT_NE(expr_str.find("+"), std::string::npos);
    EXPECT_NE(expr_str.find(">"), std::string::npos);
}

// TODO: Add more comprehensive expression tests when evaluation is implemented
// - Expression evaluation tests
// - Type checking tests
// - Error handling tests
// - Complex nested expression tests
