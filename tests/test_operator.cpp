#include <gtest/gtest.h>
#include "execution/operator.hpp"
#include "execution/expression.hpp"
#include "catalog/table.hpp"
#include "catalog/schema.hpp"
#include "types/data_type.hpp"

using namespace velodb;

class OperatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a test table with sample data
        auto int_type = std::make_unique<IntegerType>();
        auto varchar_type = std::make_unique<VarcharType>(100);
        
        Column id_col("id", std::move(int_type));
        Column name_col("name", std::move(varchar_type));
        
        auto schema = std::make_unique<Schema>();
        schema->addColumn(std::move(id_col));
        schema->addColumn(std::move(name_col));

        auto table_info = std::make_unique<TableInfo>("test_table", std::move(schema));
        test_table_ = std::make_unique<Table>(std::move(table_info));
        
        // Insert some test data
        std::vector<Value> values1;
        values1.push_back(Value::createInteger(1));
        values1.push_back(Value::createString("Alice"));
        Tuple tuple1(test_table_->getSchema(), std::move(values1));
        test_table_->insertTuple(std::move(tuple1));
        
        std::vector<Value> values2;
        values2.push_back(Value::createInteger(2));
        values2.push_back(Value::createString("Bob"));
        Tuple tuple2(test_table_->getSchema(), std::move(values2));
        test_table_->insertTuple(std::move(tuple2));
    }

    void TearDown() override {
        // Cleanup code if needed
    }

    std::unique_ptr<Table> test_table_;
};

TEST_F(OperatorTest, SeqScanOperatorCreation) {
    SeqScanOperator scan_op(*test_table_, nullptr);

    EXPECT_EQ(scan_op.getOutputSchema().getColumnCount(), 2);
    EXPECT_EQ(scan_op.getOutputSchema().getColumn(0).getName(), "id");
    EXPECT_EQ(scan_op.getOutputSchema().getColumn(1).getName(), "name");
}

TEST_F(OperatorTest, SeqScanOperatorIteration) {
    SeqScanOperator scan_op(*test_table_, nullptr);

    scan_op.init();

    Tuple tuple(scan_op.getOutputSchema());
    RowId row_id;
    
    // First tuple
    EXPECT_TRUE(scan_op.next(&tuple, &row_id));
    EXPECT_EQ(tuple.getValue(0).getInteger(), 1);
    EXPECT_EQ(tuple.getValue(1).getString(), "Alice");

    // Second tuple
    EXPECT_TRUE(scan_op.next(&tuple, &row_id));
    EXPECT_EQ(tuple.getValue(0).getInteger(), 2);
    EXPECT_EQ(tuple.getValue(1).getString(), "Bob");

    // No more tuples
    EXPECT_FALSE(scan_op.next(&tuple, &row_id));
}

TEST_F(OperatorTest, SeqScanOperatorWithPredicate) {
    // Create a predicate: id = 1
    Value target_val = Value::createInteger(1);
    auto const_expr = std::make_unique<ConstantExpression>(target_val);
    
    auto int_type = std::make_unique<IntegerType>();
    auto col_expr = std::make_unique<ColumnRefExpression>("id", std::move(int_type));
    
    auto predicate = std::make_unique<ComparisonExpression>(
        ComparisonType::EQUAL, std::move(col_expr), std::move(const_expr));
    
    SeqScanOperator scan_op(*test_table_, std::move(predicate));

    scan_op.init();

    Tuple tuple(scan_op.getOutputSchema());
    RowId row_id;
    
    // Should only get the first tuple (id = 1)
    EXPECT_TRUE(scan_op.next(&tuple, &row_id));
    EXPECT_EQ(tuple.getValue(0).getInteger(), 1);
    EXPECT_EQ(tuple.getValue(1).getString(), "Alice");

    // No more tuples should match
    EXPECT_FALSE(scan_op.next(&tuple, &row_id));
}

// TODO: Add more comprehensive operator tests when additional operators are implemented
// - ProjectionOperator tests
// - FilterOperator tests
// - JoinOperator tests
// - AggregationOperator tests
// - SortOperator tests
// - LimitOperator tests
