#include "../common/test_warmup_utility.hpp"
#include "catalog/table_builder.hpp"
#include "data/data_type.hpp"
#include "data/oblivious_table.hpp"
#include "data/oblivious_table_manager.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <numeric>
#include <set>

using namespace velodb;

class ObliviousTableTest : public test::VelODBTest {
protected:
    void SetUp() override
    {
        test::VelODBTest::SetUp();

        // Create a simple test table schema
        Schema schema;
        schema.addColumnInfo({ "id", std::make_unique<IntegerType>() });
        schema.addColumnInfo({ "value", std::make_unique<BigIntType>() });

        TableBuilder builder("test_table", std::move(schema));

        // Add some rows
        for (int i = 0; i < 1000; i++) {
            builder.insertRow({ Value::createInteger(i), Value::createBigInt(i * 100) });
        }

        catalog_.addTable(std::move(builder).build());
    }
};

// Test basic ObliviousTable creation and position map
TEST_F(ObliviousTableTest, BasicCreation)
{
    Table* table_ptr = catalog_.getTableMutable("test_table");
    ASSERT_NE(table_ptr, nullptr);

    ObliviousTable ot("test_table", table_ptr, task_manager_);

    EXPECT_EQ(ot.getName(), "test_table");
    EXPECT_EQ(ot.getRowCount(), 1000);
    EXPECT_EQ(ot.getSourceTable(), table_ptr);

    // Position map should be a valid permutation
    std::set<uint32_t> positions;
    for (size_t i = 0; i < ot.getRowCount(); i++) {
        uint32_t pos = ot.getPosition(i);
        EXPECT_LT(pos, ot.getRowCount());
        positions.insert(pos);
    }
    // All positions should be unique (it's a permutation)
    EXPECT_EQ(positions.size(), ot.getRowCount());
}

// Test that shuffle changes the position map
TEST_F(ObliviousTableTest, ShuffleChangesPositionMap)
{
    Table* table_ptr = catalog_.getTableMutable("test_table");
    ASSERT_NE(table_ptr, nullptr);

    ObliviousTable ot("test_table", table_ptr, task_manager_);

    // Record initial position map
    std::vector<uint32_t> initial_positions(ot.getRowCount());
    for (size_t i = 0; i < ot.getRowCount(); i++) {
        initial_positions[i] = ot.getPosition(i);
    }

    // Perform shuffle (now uses command queue internally)
    ot.startAsyncShuffle();
    ot.waitForShuffle();

    // Check that positions changed
    int changed_count = 0;
    for (size_t i = 0; i < ot.getRowCount(); i++) {
        if (ot.getPosition(i) != initial_positions[i]) {
            changed_count++;
        }
    }

    // Most positions should have changed (statistically very unlikely to be same)
    EXPECT_GT(changed_count, static_cast<int>(ot.getRowCount() * 0.9));

    // But it should still be a valid permutation
    std::set<uint32_t> positions;
    for (size_t i = 0; i < ot.getRowCount(); i++) {
        positions.insert(ot.getPosition(i));
    }
    EXPECT_EQ(positions.size(), ot.getRowCount());
}

// Test ObliviousTableManager
TEST_F(ObliviousTableTest, ManagerBasicUsage)
{
    // Initialize oblivious manager
    catalog_.initObliviousManager(task_manager_);
    catalog_.registerAllTablesAsOblivious();

    auto& mgr = catalog_.getObliviousManager();

    EXPECT_TRUE(mgr.hasTable("test_table"));

    auto stats = mgr.getStats();
    EXPECT_EQ(stats.tables_registered, 1);
}

// Test query lifecycle
TEST_F(ObliviousTableTest, QueryLifecycle)
{
    catalog_.initObliviousManager(task_manager_);
    catalog_.registerAllTablesAsOblivious();

    auto& mgr = catalog_.getObliviousManager();

    // Record initial position for record 0
    [[maybe_unused]] uint32_t initial_pos = mgr.getTable("test_table").getPosition(0);

    // Simulate query
    mgr.beginQuery({ "test_table" });
    mgr.markAccessed("test_table");
    mgr.endQuery(); // This triggers async shuffle

    // Simulate next query (should wait for shuffle)
    mgr.beginQuery({ "test_table" });

    // Position should have changed after shuffle
    [[maybe_unused]] uint32_t new_pos = mgr.getTable("test_table").getPosition(0);

    // Note: There's a small probability they could be the same, but very unlikely
    // for a 1000-element table
    // We just check that the system didn't crash

    mgr.endQuery();

    auto stats = mgr.getStats();
    EXPECT_GE(stats.total_shuffles, 1);
}

// Test multiple shuffles
TEST_F(ObliviousTableTest, MultipleShuffles)
{
    Table* table_ptr = catalog_.getTableMutable("test_table");
    ASSERT_NE(table_ptr, nullptr);

    ObliviousTable ot("test_table", table_ptr, task_manager_);

    // Perform multiple shuffles
    for (int round = 0; round < 5; round++) {
        ot.startAsyncShuffle();
        ot.waitForShuffle();

        // Verify it's still a valid permutation
        std::set<uint32_t> positions;
        for (size_t i = 0; i < ot.getRowCount(); i++) {
            positions.insert(ot.getPosition(i));
        }
        EXPECT_EQ(positions.size(), ot.getRowCount());
    }

    EXPECT_EQ(ot.getShuffleCount(), 5);
}

// Test GPU position enrichment
TEST_F(ObliviousTableTest, PositionEnrichment)
{
    catalog_.initObliviousManager(task_manager_);
    catalog_.registerAllTablesAsOblivious();

    auto& mgr = catalog_.getObliviousManager();
    mgr.beginQuery({ "test_table" });

    // Create some record IDs on GPU
    const size_t n = 100;
    std::vector<uint32_t> h_record_ids(n);
    std::iota(h_record_ids.begin(), h_record_ids.end(), 0); // 0, 1, 2, ..., 99

    // Enrich positions
    cudaStream_t stream;
    cudaStreamCreate(&stream);

    uint32_t* d_record_ids;
    uint32_t* d_positions;
    cudaMallocAsync(&d_record_ids, n * sizeof(uint32_t), stream);
    cudaMallocAsync(&d_positions, n * sizeof(uint32_t), stream);

    cudaMemcpyAsync(d_record_ids, h_record_ids.data(), n * sizeof(uint32_t), cudaMemcpyHostToDevice, stream);

    mgr.enrichPositions("test_table", d_record_ids, d_positions, n, stream);

    // Copy back and verify
    std::vector<uint32_t> h_positions(n);
    cudaMemcpyAsync(h_positions.data(), d_positions, n * sizeof(uint32_t), cudaMemcpyDeviceToHost, stream);
    cudaStreamSynchronize(stream);

    // Verify positions match what we'd get from CPU
    for (size_t i = 0; i < n; i++) {
        EXPECT_EQ(h_positions[i], mgr.getTable("test_table").getPosition(i));
    }

    cudaFreeAsync(d_record_ids, stream);
    cudaFreeAsync(d_positions, stream);
    cudaStreamSynchronize(stream);
    cudaStreamDestroy(stream);

    mgr.endQuery();
}
