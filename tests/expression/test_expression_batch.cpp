#include "../common/test_warmup_utility.hpp"
#include "catalog/column.hpp"
#include "catalog/row_batch.hpp"
#include "catalog/schema.hpp"
#include "data/data_type.hpp"
#include "data/value.hpp"
#include "expression/arithmetic_expression.hpp"
#include "expression/column_ref_expression.hpp"
#include "expression/comparison_expression.hpp"
#include "expression/constant_expression.hpp"
#include "expression/logical_expression.hpp"

#include <gtest/gtest.h>

#include <vector>

using namespace velodb;

class ExpressionBatchTest : public test::VelODBTest {
protected:
    void SetUp() override
    {
        test::VelODBTest::SetUp();
        setupSchemaAndBatch();
    }

    void setupSchemaAndBatch()
    {
        // Schema: id (BIGINT), val (INTEGER), active (BOOLEAN)
        std::vector<ColumnInfo> columns;
        columns.emplace_back("id", DataType::createType(DataTypeId::BIGINT));
        columns.emplace_back("val", DataType::createType(DataTypeId::INTEGER));
        columns.emplace_back("active", DataType::createType(DataTypeId::BOOLEAN));
        schema_ = Schema(std::move(columns));

        // Create data
        // id: 0, 1, 2, 3, 4
        // val: 10, 20, 5, 30, 25
        // active: true, false, true, true, false
        size_t row_count = 5;

        Column id_col(DataType::createType(DataTypeId::BIGINT), row_count);
        Column val_col(DataType::createType(DataTypeId::INTEGER), row_count);
        Column active_col(DataType::createType(DataTypeId::BOOLEAN), row_count);

        std::vector<int64_t> ids = { 0, 1, 2, 3, 4 };
        std::vector<int32_t> vals = { 10, 20, 5, 30, 25 };
        std::vector<bool> actives = { true, false, true, true, false };

        for (size_t i = 0; i < row_count; ++i) {
            id_col.append(Value::createBigInt(ids[i]));
            val_col.append(Value::createInteger(vals[i]));
            active_col.append(Value::createBoolean(actives[i]));
        }

        row_batch_ = std::make_unique<RowBatch>();
        row_batch_->addColumn(std::move(id_col));
        row_batch_->addColumn(std::move(val_col));
        row_batch_->addColumn(std::move(active_col));
    }

    Schema schema_;
    std::unique_ptr<RowBatch> row_batch_;
};

TEST_F(ExpressionBatchTest, EvaluateConstant)
{
    Value const_val = Value::createInteger(100);
    ConstantExpression expr(const_val);

    Column result = expr.evaluateBatch(*row_batch_, schema_);

    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(result.getType().getTypeId(), DataTypeId::INTEGER);

    for (size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(result.get(i).getInteger(), 100);
    }
}

TEST_F(ExpressionBatchTest, EvaluateColumnRef)
{
    // Ref "val" column
    ColumnRefExpression expr("table", "val", DataType::createType(DataTypeId::INTEGER));

    Column result = expr.evaluateBatch(*row_batch_, schema_);

    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(result.getType().getTypeId(), DataTypeId::INTEGER);

    std::vector<int32_t> expected = { 10, 20, 5, 30, 25 };
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(result.get(i).getInteger(), expected[i]);
    }
}

TEST_F(ExpressionBatchTest, EvaluateComparison)
{
    // val > 20
    auto col_ref = std::make_unique<ColumnRefExpression>("table", "val", DataType::createType(DataTypeId::INTEGER));
    auto const_val = std::make_unique<ConstantExpression>(Value::createInteger(20));

    ComparisonExpression expr(ComparisonType::GREATER_THAN, std::move(col_ref), std::move(const_val));

    Column result = expr.evaluateBatch(*row_batch_, schema_);

    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(result.getType().getTypeId(), DataTypeId::BOOLEAN);

    // 10 > 20 -> F
    // 20 > 20 -> F
    // 5 > 20  -> F
    // 30 > 20 -> T
    // 25 > 20 -> T
    std::vector<bool> expected = { false, false, false, true, true };
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(result.get(i).getBoolean(), expected[i]);
    }
}

TEST_F(ExpressionBatchTest, EvaluateComparisonVectorVector)
{
    // val > id (Integer vs BigInt - beware of type mismatch handling in your implementation.
    // The implementation of ComparisonExpression::evaluateBatch currently checks for same type ID for optimized path.
    // AbstractExpression::evaluateBatch (fallback) handles different types via Value::evaluate.
    // However, let's test a case where types match for optimization, e.g. val > val (trivial) or mock another int
    // column.

    // Let's create a new batch with two integer columns for this test specific logic or just trust simple comparison.
    // Let's use val > 15 (Vector vs Constant) which effectively is Vector vs Vector(Constant).
    // But to test Vector vs Vector fully, let's compare val against itself or similar.

    // val >= val
    auto col1 = std::make_unique<ColumnRefExpression>("table", "val", DataType::createType(DataTypeId::INTEGER));
    auto col2 = std::make_unique<ColumnRefExpression>("table", "val", DataType::createType(DataTypeId::INTEGER));

    ComparisonExpression expr(ComparisonType::GREATER_THAN_OR_EQUAL, std::move(col1), std::move(col2));

    Column result = expr.evaluateBatch(*row_batch_, schema_);
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_TRUE(result.get(i).getBoolean());
    }
}

TEST_F(ExpressionBatchTest, EvaluateLogicalAnd)
{
    // (val > 10) AND active
    // val > 10 results:
    // 10 > 10 -> F
    // 20 > 10 -> T
    // 5 > 10  -> F
    // 30 > 10 -> T
    // 25 > 10 -> T
    //
    // active: T, F, T, T, F
    //
    // AND result:
    // F && T -> F
    // T && F -> F
    // F && T -> F
    // T && T -> T
    // T && F -> F

    auto col_ref = std::make_unique<ColumnRefExpression>("table", "val", DataType::createType(DataTypeId::INTEGER));
    auto const_val = std::make_unique<ConstantExpression>(Value::createInteger(10));
    auto comp_expr = std::make_unique<ComparisonExpression>(ComparisonType::GREATER_THAN,
                                                            std::move(col_ref),
                                                            std::move(const_val));

    auto active_ref = std::make_unique<ColumnRefExpression>("table",
                                                            "active",
                                                            DataType::createType(DataTypeId::BOOLEAN));

    BinaryLogicalExpression expr(ConnectiveType::AND, std::move(comp_expr), std::move(active_ref));

    Column result = expr.evaluateBatch(*row_batch_, schema_);

    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(result.getType().getTypeId(), DataTypeId::BOOLEAN);

    std::vector<bool> expected = { false, false, false, true, false };
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(result.get(i).getBoolean(), expected[i]);
    }
}

TEST_F(ExpressionBatchTest, EvaluateLogicalNot)
{
    // NOT active
    // active: T, F, T, T, F
    // Result: F, T, F, F, T

    auto active_ref = std::make_unique<ColumnRefExpression>("table",
                                                            "active",
                                                            DataType::createType(DataTypeId::BOOLEAN));
    LogicalNotExpression expr(std::move(active_ref));

    Column result = expr.evaluateBatch(*row_batch_, schema_);

    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(result.getType().getTypeId(), DataTypeId::BOOLEAN);

    std::vector<bool> expected = { false, true, false, false, true };
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(result.get(i).getBoolean(), expected[i]);
    }
}
