#include "cuda/device_buffer.hpp"
#include "cuda/helper.hpp"
#include "cuda/oblivious_dedup.hpp"
#include "cuda/oblivious_padding.hpp"
#include "cuda/oblivious_reconstruction.hpp"
#include "cuda/stream_pool.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <vector>

using namespace velodb;

class ObliviousPrimitivesTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Initialize CUDA stream
        auto res = StreamPool::instance().acquire();
        ASSERT_TRUE(res);
        stream_handle_ = std::move(res.value());
        stream_ = stream_handle_->get()->get();
    }

    void TearDown() override
    {
        if (stream_handle_) {
            stream_handle_->release();
        }
    }

    std::optional<StreamPool::StreamHandle> stream_handle_;
    cudaStream_t stream_;
};

TEST_F(ObliviousPrimitivesTest, DeduplicateRowids)
{
    std::vector<uint64_t> input = { 1, 5, 2, 5, 3, 1, 4 };
    size_t count = input.size();

    DeviceBuffer d_input(count * sizeof(uint64_t), stream_);
    d_input.copyFromHostAsync(input.data(), count * sizeof(uint64_t), stream_);

    auto result = cuda::deduplicateRowids(d_input, count, stream_);
    ASSERT_TRUE(result);

    auto [d_unique, unique_count] = std::move(result.value());

    ASSERT_EQ(unique_count, 5); // 1, 2, 3, 4, 5

    std::vector<uint64_t> h_unique(unique_count);
    d_unique.copyToHostAsync(h_unique.data(), unique_count * sizeof(uint64_t), stream_);
    cudaStreamSynchronize(stream_);

    std::sort(h_unique.begin(), h_unique.end());
    std::vector<uint64_t> expected = { 1, 2, 3, 4, 5 };
    ASSERT_EQ(h_unique, expected);
}

TEST_F(ObliviousPrimitivesTest, PadAndShuffle)
{
    std::vector<uint64_t> unique_input = { 10, 20, 30 };
    size_t unique_count = unique_input.size();
    size_t target_size = 10;

    DeviceBuffer d_input(unique_count * sizeof(uint64_t), stream_);
    d_input.copyFromHostAsync(unique_input.data(), unique_count * sizeof(uint64_t), stream_);

    auto result = cuda::padAndShuffleRowids(d_input, unique_count, target_size, 12345, stream_);
    ASSERT_TRUE(result);

    auto padding_res = std::move(result.value());
    ASSERT_EQ(padding_res.padded_count, target_size);

    std::vector<uint64_t> h_padded(target_size);
    std::vector<uint8_t> h_mask_bytes(target_size); // bool is 1 byte

    padding_res.padded_rowids.copyToHostAsync(h_padded.data(), target_size * sizeof(uint64_t), stream_);
    padding_res.real_mask.copyToHostAsync(h_mask_bytes.data(), target_size * sizeof(bool), stream_);
    cudaStreamSynchronize(stream_);

    size_t real_count = 0;
    std::vector<uint64_t> recovered_ids;
    for (size_t i = 0; i < target_size; ++i) {
        if (h_mask_bytes[i]) {
            real_count++;
            recovered_ids.push_back(h_padded[i]);
        }
    }

    ASSERT_EQ(real_count, unique_count);
    std::sort(recovered_ids.begin(), recovered_ids.end());
    ASSERT_EQ(recovered_ids, unique_input);
}

TEST_F(ObliviousPrimitivesTest, Reconstruction)
{
    // 1. Simulate Padded RowIDs (Index -> RowID)
    // Let's say we have 3 real rows: 100, 200, 300.
    // Target size 5.
    // Padded/Shuffled: [0(dummy), 200, 0(dummy), 100, 300]
    // Mask: [0, 1, 0, 1, 1]

    size_t target_size = 5;
    std::vector<uint64_t> h_padded_rowids = { 0, 200, 0, 100, 300 };

    DeviceBuffer d_padded_rowids(target_size * sizeof(uint64_t), stream_);
    d_padded_rowids.copyFromHostAsync(h_padded_rowids.data(), target_size * sizeof(uint64_t), stream_);

    // 2. Build Lookup Map
    auto map_res = cuda::buildLookupMap(d_padded_rowids, target_size, stream_);
    ASSERT_TRUE(map_res);
    auto d_map = std::move(map_res.value());

    // 3. Simulate Fetched Data (CPU -> GPU)
    // The CPU fetched data corresponding to h_padded_rowids order.
    // Row 100 -> Value 10
    // Row 200 -> Value 20
    // Row 300 -> Value 30
    // Dummy -> Value 0
    // Fetched Buffer: [0, 20, 0, 10, 30] (int64_t)
    std::vector<int64_t> h_fetched_data = { 0, 20, 0, 10, 30 };
    DeviceBuffer d_fetched_data(target_size * sizeof(int64_t), stream_);
    d_fetched_data.copyFromHostAsync(h_fetched_data.data(), target_size * sizeof(int64_t), stream_);

    // 4. Original Join Result (RowIDs we want to reconstruct)
    // Let's say the join result was: [100, 300, 200, 100]
    std::vector<uint64_t> h_join_rowids = { 100, 300, 200, 100 };
    size_t result_count = h_join_rowids.size();
    DeviceBuffer d_join_rowids(result_count * sizeof(uint64_t), stream_);
    d_join_rowids.copyFromHostAsync(h_join_rowids.data(), result_count * sizeof(uint64_t), stream_);

    // 5. Gather
    DeviceBuffer d_output(result_count * sizeof(int64_t), stream_);

    auto gather_res
        = cuda::gatherColumn(d_output, d_fetched_data, 0, d_map, d_join_rowids, result_count, sizeof(int64_t), stream_);
    ASSERT_TRUE(gather_res);

    std::vector<int64_t> h_output(result_count);
    d_output.copyToHostAsync(h_output.data(), result_count * sizeof(int64_t), stream_);
    cudaStreamSynchronize(stream_);

    // Expected: [10, 30, 20, 10]
    std::vector<int64_t> expected = { 10, 30, 20, 10 };
    ASSERT_EQ(h_output, expected);
}
