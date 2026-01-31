#include "../common/test_warmup_utility.hpp"
#include "catalog/execution_context.hpp"
#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include "catalog/table_builder.hpp"
#include "data/data_type.hpp"
#include "data/oblivious_table.hpp"
#include "data/oblivious_table_manager.hpp"
#include "operator/materialization_operator.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <numeric>
#include <random>
#include <set>

using namespace velodb;

// ============================================================================
// Mock Operator to provide rowid batches for testing MaterializationOperator
// ============================================================================

class MockRowIdOperator : public AbstractOperator {
public:
    MockRowIdOperator(ExecutionContext& context, Schema output_schema, std::vector<std::vector<int64_t>> table_rowids)
        : AbstractOperator(context, std::move(output_schema))
        , table_rowids_(std::move(table_rowids))
    {
    }

    bool isUnary() const override { return false; }

    Result<RowBatch> next() override
    {
        if (returned_) {
            return Result<RowBatch>::success(RowBatch()); // End of stream
        }
        returned_ = true;

        // Build batch with rowid columns
        std::vector<Column> columns;

        for (const auto& rowids : table_rowids_) {
            std::vector<Value> values;
            for (int64_t rid : rowids) {
                values.push_back(Value::createBigInt(rid));
            }
            columns.push_back(Column::buildFrom(std::make_unique<BigIntType>(), std::move(values)));
        }

        RowBatch batch = buildBatchFromColumns(std::move(columns));
        setNumRowsForBatch(batch, table_rowids_[0].size());
        return Result<RowBatch>::success(std::move(batch));
    }

private:
    std::vector<std::vector<int64_t>> table_rowids_;
    bool returned_ = false;
};

// ============================================================================
// Test Fixture
// ============================================================================

class ObliviousMaterializationTest : public test::VelODBTest {
protected:
    void SetUp() override
    {
        test::VelODBTest::SetUp();

        // Create orders table
        {
            Schema schema;
            schema.addColumnInfo({ "o_orderkey", std::make_unique<BigIntType>() });
            schema.addColumnInfo({ "o_custkey", std::make_unique<BigIntType>() });
            schema.addColumnInfo({ "o_totalprice", std::make_unique<BigIntType>() });

            TableBuilder builder("orders", std::move(schema));
            for (int i = 0; i < 100; i++) {
                builder.insertRow({ Value::createBigInt(i * 10), // o_orderkey
                                    Value::createBigInt(i % 20), // o_custkey
                                    Value::createBigInt((i + 1) * 100) }); // o_totalprice
            }
            catalog_.addTable(std::move(builder).build());
        }

        // Create lineitem table
        {
            Schema schema;
            schema.addColumnInfo({ "l_orderkey", std::make_unique<BigIntType>() });
            schema.addColumnInfo({ "l_linenumber", std::make_unique<BigIntType>() });
            schema.addColumnInfo({ "l_quantity", std::make_unique<BigIntType>() });
            schema.addColumnInfo({ "l_extendedprice", std::make_unique<BigIntType>() });

            TableBuilder builder("lineitem", std::move(schema));
            for (int i = 0; i < 500; i++) {
                builder.insertRow({ Value::createBigInt((i / 5) * 10), // l_orderkey (5 lineitems per order)
                                    Value::createBigInt(i % 5), // l_linenumber
                                    Value::createBigInt(i % 10 + 1), // l_quantity
                                    Value::createBigInt(i * 50) }); // l_extendedprice
            }
            catalog_.addTable(std::move(builder).build());
        }
    }
};

// ============================================================================
// Test: Single Table Recovery Correctness
// ============================================================================

TEST_F(ObliviousMaterializationTest, SingleTableRecoveryCorrectness)
{
    // Simulate recovering from orders table
    // Row IDs: [0, 5, 10, 5, 0] - includes duplicates to test deduplication
    std::vector<int64_t> orders_rowids = { 0, 5, 10, 5, 0 };

    // Build input schema (rowid columns)
    Schema input_schema;
    input_schema.addColumnInfo({ "orders.$_rowid", std::make_unique<BigIntType>() });

    // Build output schema (columns we want to materialize)
    Schema output_schema;
    output_schema.addColumnInfo({ "orders.o_orderkey", std::make_unique<BigIntType>() });
    output_schema.addColumnInfo({ "orders.o_totalprice", std::make_unique<BigIntType>() });

    auto context = ExecutionContext(catalog_, task_manager_);
    auto mock_input = std::make_unique<MockRowIdOperator>(context,
                                                          std::move(input_schema),
                                                          std::vector<std::vector<int64_t>> { orders_rowids });

    MaterializationOperator mat_op(context, std::move(output_schema), std::move(mock_input));

    auto result = mat_op.next();
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();

    auto batch = std::move(result.value());
    batch.to(DataLocation::HOST_PAGEABLE);

    EXPECT_EQ(batch.getRowCount(), 5);

    // Verify values match expected
    // Row 0: o_orderkey=0, o_totalprice=100
    // Row 5: o_orderkey=50, o_totalprice=600
    // Row 10: o_orderkey=100, o_totalprice=1100

    EXPECT_EQ(batch.getValue(0, 0).get<int64_t>(), 0); // orders[0].o_orderkey
    EXPECT_EQ(batch.getValue(0, 1).get<int64_t>(), 100); // orders[0].o_totalprice

    EXPECT_EQ(batch.getValue(1, 0).get<int64_t>(), 50); // orders[5].o_orderkey
    EXPECT_EQ(batch.getValue(1, 1).get<int64_t>(), 600); // orders[5].o_totalprice

    EXPECT_EQ(batch.getValue(2, 0).get<int64_t>(), 100); // orders[10].o_orderkey
    EXPECT_EQ(batch.getValue(2, 1).get<int64_t>(), 1100); // orders[10].o_totalprice

    // Duplicates should have same values
    EXPECT_EQ(batch.getValue(3, 0).get<int64_t>(), 50); // orders[5] again
    EXPECT_EQ(batch.getValue(3, 1).get<int64_t>(), 600);

    EXPECT_EQ(batch.getValue(4, 0).get<int64_t>(), 0); // orders[0] again
    EXPECT_EQ(batch.getValue(4, 1).get<int64_t>(), 100);
}

// ============================================================================
// Test: Multi-Table Recovery Preserves Pairing
// ============================================================================

TEST_F(ObliviousMaterializationTest, MultiTablePairingPreserved)
{
    // Simulate JOIN result: orders[i] JOIN lineitem[j]
    // Pairs: (0, 0), (0, 1), (5, 25), (5, 26), (10, 50)
    std::vector<int64_t> orders_rowids = { 0, 0, 5, 5, 10 };
    std::vector<int64_t> lineitem_rowids = { 0, 1, 25, 26, 50 };

    // Build input schema
    Schema input_schema;
    input_schema.addColumnInfo({ "orders.$_rowid", std::make_unique<BigIntType>() });
    input_schema.addColumnInfo({ "lineitem.$_rowid", std::make_unique<BigIntType>() });

    // Build output schema
    Schema output_schema;
    output_schema.addColumnInfo({ "orders.o_orderkey", std::make_unique<BigIntType>() });
    output_schema.addColumnInfo({ "orders.o_totalprice", std::make_unique<BigIntType>() });
    output_schema.addColumnInfo({ "lineitem.l_linenumber", std::make_unique<BigIntType>() });
    output_schema.addColumnInfo({ "lineitem.l_extendedprice", std::make_unique<BigIntType>() });

    auto context = ExecutionContext(catalog_, task_manager_);
    auto mock_input = std::make_unique<MockRowIdOperator>(
        context,
        std::move(input_schema),
        std::vector<std::vector<int64_t>> { orders_rowids, lineitem_rowids });

    MaterializationOperator mat_op(context, std::move(output_schema), std::move(mock_input));

    auto result = mat_op.next();
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();

    auto batch = std::move(result.value());
    batch.to(DataLocation::HOST_PAGEABLE);

    EXPECT_EQ(batch.getRowCount(), 5);

    // Verify pairing is preserved:
    // Row 0: orders[0] + lineitem[0]
    EXPECT_EQ(batch.getValue(0, 0).get<int64_t>(), 0); // orders[0].o_orderkey
    EXPECT_EQ(batch.getValue(0, 2).get<int64_t>(), 0); // lineitem[0].l_linenumber

    // Row 1: orders[0] + lineitem[1]
    EXPECT_EQ(batch.getValue(1, 0).get<int64_t>(), 0); // orders[0].o_orderkey (same order)
    EXPECT_EQ(batch.getValue(1, 2).get<int64_t>(), 1); // lineitem[1].l_linenumber (different)

    // Row 2: orders[5] + lineitem[25]
    EXPECT_EQ(batch.getValue(2, 0).get<int64_t>(), 50); // orders[5].o_orderkey
    EXPECT_EQ(batch.getValue(2, 2).get<int64_t>(), 0); // lineitem[25].l_linenumber (25 % 5 = 0)

    // Row 3: orders[5] + lineitem[26]
    EXPECT_EQ(batch.getValue(3, 0).get<int64_t>(), 50); // orders[5].o_orderkey (same)
    EXPECT_EQ(batch.getValue(3, 2).get<int64_t>(), 1); // lineitem[26].l_linenumber (26 % 5 = 1)

    // Row 4: orders[10] + lineitem[50]
    EXPECT_EQ(batch.getValue(4, 0).get<int64_t>(), 100); // orders[10].o_orderkey
    EXPECT_EQ(batch.getValue(4, 2).get<int64_t>(), 0); // lineitem[50].l_linenumber (50 % 5 = 0)
}

// ============================================================================
// Test: Integration with Oblivious Table Manager
// ============================================================================

TEST_F(ObliviousMaterializationTest, IntegrationWithObliviousManager)
{
    // Initialize oblivious manager
    catalog_.initObliviousManager(task_manager_);
    catalog_.registerAllTablesAsOblivious();

    auto& mgr = catalog_.getObliviousManager();
    mgr.beginQuery({ "orders" });
    mgr.markAccessed("orders");

    // Test with duplicate logical rowids to verify consistency
    // With oblivious table, logical rowid maps to shuffled physical position,
    // but same logical rowid should always return same data within a query.
    std::vector<int64_t> orders_rowids = { 0, 10, 0, 10 }; // Duplicates

    Schema input_schema;
    input_schema.addColumnInfo({ "orders.$_rowid", std::make_unique<BigIntType>() });

    Schema output_schema;
    output_schema.addColumnInfo({ "orders.o_orderkey", std::make_unique<BigIntType>() });
    output_schema.addColumnInfo({ "orders.o_totalprice", std::make_unique<BigIntType>() });

    auto context = ExecutionContext(catalog_, task_manager_);
    auto mock_input = std::make_unique<MockRowIdOperator>(context,
                                                          std::move(input_schema),
                                                          std::vector<std::vector<int64_t>> { orders_rowids });

    MaterializationOperator mat_op(context, std::move(output_schema), std::move(mock_input));

    auto result = mat_op.next();
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();

    auto batch = std::move(result.value());
    batch.to(DataLocation::HOST_PAGEABLE);

    EXPECT_EQ(batch.getRowCount(), 4);

    // Key verification: same logical rowid should return same data
    // Row 0 and Row 2 both have logical rowid 0 -> should be identical
    EXPECT_EQ(batch.getValue(0, 0).get<int64_t>(), batch.getValue(2, 0).get<int64_t>());
    EXPECT_EQ(batch.getValue(0, 1).get<int64_t>(), batch.getValue(2, 1).get<int64_t>());

    // Row 1 and Row 3 both have logical rowid 10 -> should be identical
    EXPECT_EQ(batch.getValue(1, 0).get<int64_t>(), batch.getValue(3, 0).get<int64_t>());
    EXPECT_EQ(batch.getValue(1, 1).get<int64_t>(), batch.getValue(3, 1).get<int64_t>());

    // Row 0 and Row 1 have different logical rowids -> should be different
    // (unless by chance they map to same physical row, which is extremely unlikely)
    // We just verify the data is valid (within expected range)
    int64_t val0 = batch.getValue(0, 0).get<int64_t>();
    int64_t val1 = batch.getValue(1, 0).get<int64_t>();

    // o_orderkey = i * 10, so valid values are 0, 10, 20, ..., 990
    EXPECT_GE(val0, 0);
    EXPECT_LT(val0, 1000);
    EXPECT_EQ(val0 % 10, 0);

    EXPECT_GE(val1, 0);
    EXPECT_LT(val1, 1000);
    EXPECT_EQ(val1 % 10, 0);

    mgr.endQuery();
}

// ============================================================================
// Test: Large Scale with Many Duplicates
// ============================================================================

TEST_F(ObliviousMaterializationTest, LargeScaleWithDuplicates)
{
    // Create rowids with many duplicates (simulating 1-to-many join)
    std::vector<int64_t> orders_rowids;
    std::vector<int64_t> lineitem_rowids;

    // Each order joins with multiple lineitems
    for (int order = 0; order < 20; order++) {
        for (int item = 0; item < 5; item++) {
            orders_rowids.push_back(order);
            lineitem_rowids.push_back(order * 5 + item);
        }
    }

    Schema input_schema;
    input_schema.addColumnInfo({ "orders.$_rowid", std::make_unique<BigIntType>() });
    input_schema.addColumnInfo({ "lineitem.$_rowid", std::make_unique<BigIntType>() });

    Schema output_schema;
    output_schema.addColumnInfo({ "orders.o_orderkey", std::make_unique<BigIntType>() });
    output_schema.addColumnInfo({ "lineitem.l_extendedprice", std::make_unique<BigIntType>() });

    auto context = ExecutionContext(catalog_, task_manager_);
    auto mock_input = std::make_unique<MockRowIdOperator>(
        context,
        std::move(input_schema),
        std::vector<std::vector<int64_t>> { orders_rowids, lineitem_rowids });

    MaterializationOperator mat_op(context, std::move(output_schema), std::move(mock_input));

    auto result = mat_op.next();
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();

    auto batch = std::move(result.value());
    batch.to(DataLocation::HOST_PAGEABLE);

    EXPECT_EQ(batch.getRowCount(), 100); // 20 orders * 5 items

    // Verify a few samples
    // Row 0: orders[0], lineitem[0]
    EXPECT_EQ(batch.getValue(0, 0).get<int64_t>(), 0); // orders[0].o_orderkey

    // Row 5: orders[1], lineitem[5]
    EXPECT_EQ(batch.getValue(5, 0).get<int64_t>(), 10); // orders[1].o_orderkey

    // Row 99: orders[19], lineitem[99]
    EXPECT_EQ(batch.getValue(99, 0).get<int64_t>(), 190); // orders[19].o_orderkey
}

// ============================================================================
// Test: Empty Input
// ============================================================================

TEST_F(ObliviousMaterializationTest, EmptyInput)
{
    std::vector<int64_t> orders_rowids = {};

    Schema input_schema;
    input_schema.addColumnInfo({ "orders.$_rowid", std::make_unique<BigIntType>() });

    Schema output_schema;
    output_schema.addColumnInfo({ "orders.o_orderkey", std::make_unique<BigIntType>() });

    auto context = ExecutionContext(catalog_, task_manager_);

    // Create a mock that returns empty batch
    class EmptyMockOperator : public AbstractOperator {
    public:
        EmptyMockOperator(ExecutionContext& ctx, Schema schema)
            : AbstractOperator(ctx, std::move(schema))
        {
        }
        bool isUnary() const override { return false; }
        Result<RowBatch> next() override { return Result<RowBatch>::success(RowBatch()); }
    };

    auto mock_input = std::make_unique<EmptyMockOperator>(context, std::move(input_schema));
    MaterializationOperator mat_op(context, std::move(output_schema), std::move(mock_input));

    auto result = mat_op.next();
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();

    auto batch = std::move(result.value());
    EXPECT_EQ(batch.getRowCount(), 0);
}
