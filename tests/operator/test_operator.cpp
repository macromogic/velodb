#include "../common/test_warmup_utility.hpp"
#include "catalog/execution_context.hpp"
#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include "catalog/table_builder.hpp"
#include "data/data_type.hpp"
#include "expression/column_ref_expression.hpp"
#include "expression/comparison_expression.hpp"
#include "expression/constant_expression.hpp"
#include "expression/expression.hpp"
#include "operator/filter_compaction_operator.hpp"
#include "operator/seq_scan_operator.hpp"

#include <gtest/gtest.h>

using namespace velodb;

class OperatorTest : public test::VelODBTest {
protected:
    void SetUp() override
    {
        test::VelODBTest::SetUp();
        // Create a test table with sample data
        auto schema = Schema();
        schema.addColumnInfo({ "id", std::make_unique<IntegerType>() });
        schema.addColumnInfo({ "name", std::make_unique<VarcharType>(100) });

        auto builder = TableBuilder("test_table", std::move(schema));

        // Insert some test data using column-based API
        std::vector<Value> values1;
        values1.push_back(Value::createInteger(1));
        values1.push_back(Value::createString("Alice"));
        builder.insertRow(values1);

        std::vector<Value> values2;
        values2.push_back(Value::createInteger(2));
        values2.push_back(Value::createString("Bob"));
        builder.insertRow(values2);

        catalog_.addTable(std::move(builder).build());
    }

    void TearDown() override
    {
        test::VelODBTest::TearDown();
        // Cleanup code if needed
    }
};

TEST_F(OperatorTest, SeqScanOperatorCreation)
{
    auto table_opt = catalog_.getTable("test_table");
    auto& test_table = table_opt.value().get();
    auto schema = test_table.getSchema().clone();
    schema.addColumnInfo({ "$_rowid", std::make_unique<BigIntType>(), false });
    schema.addColumnInfo({ "$_mask", std::make_unique<BooleanType>(), false });
    auto context = ExecutionContext(catalog_, task_manager_);
    SeqScanOperator scan_op(context, test_table, std::move(schema), nullptr);

    EXPECT_EQ(scan_op.getOutputSchema().getColumnCount(), 4);
    EXPECT_EQ(scan_op.getOutputSchema().getColumnInfo(0).getName(), "id");
    EXPECT_EQ(scan_op.getOutputSchema().getColumnInfo(1).getName(), "name");
    EXPECT_EQ(scan_op.getOutputSchema().getColumnInfo(2).getName(), "$_rowid");
    EXPECT_EQ(scan_op.getOutputSchema().getColumnInfo(3).getName(), "$_mask");
}

TEST_F(OperatorTest, SeqScanOperatorWithPredicate)
{
    // Create a predicate: id = 1
    Value target_val = Value::createInteger(1);
    auto const_expr = std::make_unique<ConstantExpression>(target_val);

    auto int_type = std::make_unique<IntegerType>();
    auto col_expr = std::make_unique<ColumnRefExpression>("test_table", "id", std::move(int_type));

    std::unique_ptr<AbstractExpression> predicate = std::make_unique<ComparisonExpression>(ComparisonType::EQUAL,
                                                                                           std::move(col_expr),
                                                                                           std::move(const_expr));

    auto table_opt = catalog_.getTable("test_table");
    auto& test_table = table_opt.value().get();
    auto context = ExecutionContext(catalog_, task_manager_);
    auto schema = test_table.getSchema().clone();
    schema.addColumnInfo({ "$_rowid", std::make_unique<BigIntType>(), false });
    schema.addColumnInfo({ "$_mask", std::make_unique<BooleanType>(), false });
    auto scan_op = std::make_unique<SeqScanOperator>(context, test_table, std::move(schema), std::move(predicate));
    auto filter_compaction_op = std::make_unique<FilterCompactionOperator>(context,
                                                                           scan_op->getOutputSchema().clone(),
                                                                           std::move(scan_op));

    auto view_result = filter_compaction_op->next();
    ASSERT_TRUE(static_cast<bool>(view_result)) << view_result.error();
    auto view = std::move(view_result.value());
    view.to(DataLocation::HOST);

    // Should only get one tuple (id = 1)
    EXPECT_EQ(view.getRowCount(), 1);
    if (view.getRowCount() >= 1) {
        EXPECT_EQ(view.getValue(0, 0).getInteger(), 1);
        EXPECT_EQ(view.getValue(0, 1).getString(), "Alice");
    }
}

// TODO: Add more comprehensive operator tests when additional operators are
// implemented
// - ProjectionOperator tests
// - FilterOperator tests
// - JoinOperator tests
// - AggregationOperator tests
// - SortOperator tests
// - LimitOperator tests
