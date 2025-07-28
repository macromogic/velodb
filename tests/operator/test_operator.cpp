#include <gtest/gtest.h>
#include "operator/scan_filter_operator.hpp"
#include "operator/compaction_operator.hpp"
#include "expression/expression.hpp"
#include "catalog/table.hpp"
#include "catalog/schema.hpp"
#include "types/data_type.hpp"

using namespace velodb;

class OperatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a test table with sample data
        auto schema = std::make_unique<Schema>();
        schema->addColumnInfo({ "id", std::make_unique<IntegerType>() });
        schema->addColumnInfo({ "name", std::make_unique<VarcharType>(100) });

        catalog_ = std::make_unique<Catalog>();
        catalog_->createTable("test_table", std::move(schema));
        auto& test_table = catalog_->getTable("test_table").value().get();

        // Insert some test data using column-based API
        std::vector<Value> values1;
        values1.push_back(Value::createInteger(1));
        values1.push_back(Value::createString("Alice"));
        test_table.insertRow(values1);
        
        std::vector<Value> values2;
        values2.push_back(Value::createInteger(2));
        values2.push_back(Value::createString("Bob"));
        test_table.insertRow(values2);
    }

    void TearDown() override {
        // Cleanup code if needed
    }

    std::unique_ptr<Catalog> catalog_; // Mock catalog for operator creation
};

TEST_F(OperatorTest, ScanFilterOperatorCreation) {
    auto& test_table = catalog_->getTable("test_table").value().get();
    ScanFilterOperator scan_op(*catalog_, test_table, nullptr);

    EXPECT_EQ(scan_op.getOutputSchema().getColumnCount(), 2);
    EXPECT_EQ(scan_op.getOutputSchema().getColumnInfo(0).getName(), "id");
    EXPECT_EQ(scan_op.getOutputSchema().getColumnInfo(1).getName(), "name");
}

TEST_F(OperatorTest, ScanFilterOperatorWithPredicate) {
    // Create a predicate: id = 1
    Value target_val = Value::createInteger(1);
    auto const_expr = std::make_unique<ConstantExpression>(target_val);
    
    auto int_type = std::make_unique<IntegerType>();
    auto col_expr = std::make_unique<ColumnRefExpression>("id", std::move(int_type));
    
    std::unique_ptr<AbstractExpression> predicate = std::make_unique<ComparisonExpression>(
        ComparisonType::EQUAL, std::move(col_expr), std::move(const_expr));
    
    auto& test_table = catalog_->getTable("test_table").value().get();
    auto scan_op = std::make_unique<ScanFilterOperator>(*catalog_, test_table, std::move(predicate));
    auto compaction_op = std::make_unique<CompactionOperator>(*catalog_, scan_op->getOutputSchema().cloneUnique(), std::move(scan_op));

    auto view_result = compaction_op->execute();
    EXPECT_TRUE(view_result.ok());
    auto view = std::move(view_result.value());
    
    // Should only get one tuple (id = 1)
    EXPECT_EQ(view.getRowCount(), 1);
    if (view.getRowCount() >= 1) {
        EXPECT_EQ(view.getValue(0, 0).getInteger(), 1);
        EXPECT_EQ(view.getValue(0, 1).getString(), "Alice");
    }
}

// TODO: Add more comprehensive operator tests when additional operators are implemented
// - ProjectionOperator tests
// - FilterOperator tests
// - JoinOperator tests
// - AggregationOperator tests
// - SortOperator tests
// - LimitOperator tests
