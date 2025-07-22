#include <gtest/gtest.h>
#include "execution/operator/scan_filter_operator.hpp"
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
        
        // Insert some test data using column-based API
        std::vector<Value> values1;
        values1.push_back(Value::createInteger(1));
        values1.push_back(Value::createString("Alice"));
        test_table_->insertRow(values1);
        
        std::vector<Value> values2;
        values2.push_back(Value::createInteger(2));
        values2.push_back(Value::createString("Bob"));
        test_table_->insertRow(values2);
    }

    void TearDown() override {
        // Cleanup code if needed
    }

    std::unique_ptr<Table> test_table_;
};

TEST_F(OperatorTest, ScanFilterOperatorCreation) {
    ScanFilterOperator scan_op(*test_table_, nullptr);

    EXPECT_EQ(scan_op.getOutputSchema().getColumnCount(), 2);
    EXPECT_EQ(scan_op.getOutputSchema().getColumn(0).getName(), "id");
    EXPECT_EQ(scan_op.getOutputSchema().getColumn(1).getName(), "name");
}

TEST_F(OperatorTest, ScanFilterOperatorIteration) {
    ScanFilterOperator scan_op(*test_table_, nullptr);

    scan_op.init();

    RowId row_id;
    std::vector<RowId> row_ids;
    
    // Collect all row IDs
    while (scan_op.nextRowId(&row_id)) {
        row_ids.push_back(row_id);
    }
    
    // Manually materialize the tuples from the table using column-based access
    std::vector<Tuple> tuples;
    for (const RowId& rid : row_ids) {
        std::vector<Value> values;
        values.reserve(test_table_->getSchema().getColumnCount());
        for (size_t col_idx = 0; col_idx < test_table_->getSchema().getColumnCount(); ++col_idx) {
            values.push_back(test_table_->getValue(rid, col_idx));
        }
        tuples.emplace_back(test_table_->getSchema(), std::move(values));
    }
    
    // Check that we got both tuples
    EXPECT_EQ(tuples.size(), 2);
    if (tuples.size() >= 2) {
        EXPECT_EQ(tuples[0].getValue(0).getInteger(), 1);
        EXPECT_EQ(tuples[0].getValue(1).getString(), "Alice");
        EXPECT_EQ(tuples[1].getValue(0).getInteger(), 2);
        EXPECT_EQ(tuples[1].getValue(1).getString(), "Bob");
    }
}

TEST_F(OperatorTest, ScanFilterOperatorWithPredicate) {
    // Create a predicate: id = 1
    Value target_val = Value::createInteger(1);
    auto const_expr = std::make_unique<ConstantExpression>(target_val);
    
    auto int_type = std::make_unique<IntegerType>();
    auto col_expr = std::make_unique<ColumnRefExpression>("id", std::move(int_type));
    
    std::unique_ptr<AbstractExpression> predicate = std::make_unique<ComparisonExpression>(
        ComparisonType::EQUAL, std::move(col_expr), std::move(const_expr));
    
    ScanFilterOperator scan_op(*test_table_, predicate);

    scan_op.init();

    RowId row_id;
    std::vector<RowId> row_ids;
    
    // Collect all row IDs that match the predicate
    while (scan_op.nextRowId(&row_id)) {
        row_ids.push_back(row_id);
    }
    
    // Manually materialize the tuples from the table using column-based access
    std::vector<Tuple> tuples;
    for (const RowId& rid : row_ids) {
        std::vector<Value> values;
        values.reserve(test_table_->getSchema().getColumnCount());
        for (size_t col_idx = 0; col_idx < test_table_->getSchema().getColumnCount(); ++col_idx) {
            values.push_back(test_table_->getValue(rid, col_idx));
        }
        tuples.emplace_back(test_table_->getSchema(), std::move(values));
    }
    
    // Should only get one tuple (id = 1)
    EXPECT_EQ(tuples.size(), 1);
    if (tuples.size() >= 1) {
        EXPECT_EQ(tuples[0].getValue(0).getInteger(), 1);
        EXPECT_EQ(tuples[0].getValue(1).getString(), "Alice");
    }
}

// TODO: Add more comprehensive operator tests when additional operators are implemented
// - ProjectionOperator tests
// - FilterOperator tests
// - JoinOperator tests
// - AggregationOperator tests
// - SortOperator tests
// - LimitOperator tests
