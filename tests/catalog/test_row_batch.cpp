#include "../common/test_warmup_utility.hpp"
#include "catalog/column.hpp"
#include "catalog/row_batch.hpp"
#include "data/data_location.hpp"
#include "data/data_type.hpp"
#include "data/value.hpp"

#include <gtest/gtest.h>

using namespace velodb;

class RowBatchTest : public test::VeloDBTest {
protected:
    void SetUp() override { test::VeloDBTest::SetUp(); }
};

TEST_F(RowBatchTest, BasicConstruction)
{
    RowBatch batch;
    EXPECT_EQ(batch.getRowCount(), 0);
    EXPECT_EQ(batch.getColumnCount(), 0);
}

TEST_F(RowBatchTest, AddColumn)
{
    RowBatch batch;

    std::vector<Value> values;
    values.push_back(Value::createInteger(1));
    values.push_back(Value::createInteger(2));
    values.push_back(Value::createInteger(3));

    auto col = Column::buildFrom(std::make_unique<IntegerType>(), std::move(values));
    batch.addColumn(std::move(col));

    EXPECT_EQ(batch.getRowCount(), 3);
    EXPECT_EQ(batch.getColumnCount(), 1);

    // Add another column
    std::vector<Value> values2;
    values2.push_back(Value::createInteger(10));
    values2.push_back(Value::createInteger(20));
    values2.push_back(Value::createInteger(30));

    auto col2 = Column::buildFrom(std::make_unique<IntegerType>(), std::move(values2));
    batch.addColumn(std::move(col2));

    EXPECT_EQ(batch.getRowCount(), 3);
    EXPECT_EQ(batch.getColumnCount(), 2);
}

TEST_F(RowBatchTest, MoveSemantics)
{
    RowBatch batch;
    std::vector<Value> values;
    values.push_back(Value::createInteger(1));
    auto col = Column::buildFrom(std::make_unique<IntegerType>(), std::move(values));
    batch.addColumn(std::move(col));

    RowBatch batch2 = std::move(batch);
    EXPECT_EQ(batch2.getRowCount(), 1);
    EXPECT_EQ(batch2.getColumnCount(), 1);
    // Moved from state is implementation defined, but usually empty
    // Our implementation checks columns_.empty() for row count 0
    EXPECT_EQ(batch.getColumnCount(), 0);
}

TEST_F(RowBatchTest, SplitFront)
{
    RowBatch batch;
    std::vector<Value> values;
    for (int i = 0; i < 10; ++i)
        values.push_back(Value::createInteger(i));

    auto col = Column::buildFrom(std::make_unique<IntegerType>(), std::move(values));
    batch.addColumn(std::move(col));

    RowBatch front = batch.splitFront(4);

    EXPECT_EQ(front.getRowCount(), 4);
    EXPECT_EQ(batch.getRowCount(), 6);

    EXPECT_EQ(front.getValue(0, 0).get<int32_t>(), 0);
    EXPECT_EQ(front.getValue(3, 0).get<int32_t>(), 3);
    EXPECT_EQ(batch.getValue(0, 0).get<int32_t>(), 4);
}

TEST_F(RowBatchTest, DataLocation)
{
    RowBatch batch;
    std::vector<Value> values;
    values.push_back(Value::createInteger(1));
    auto col = Column::buildFrom(std::make_unique<IntegerType>(), std::move(values));
    batch.addColumn(std::move(col));

    // Default is HOST
    EXPECT_EQ(batch.getColumn(0).location(), DataLocation::HOST);

    // Move to DEVICE
    batch.to(DataLocation::CUDA);
    EXPECT_EQ(batch.getColumn(0).location(), DataLocation::CUDA);

    // Move back to HOST
    batch.to(DataLocation::HOST);
    EXPECT_EQ(batch.getColumn(0).location(), DataLocation::HOST);
    EXPECT_EQ(batch.getValue(0, 0).get<int32_t>(), 1);
}
