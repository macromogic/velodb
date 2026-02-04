#pragma once

#include "catalog/table.hpp"
#include "common/copy_traits.hpp"
#include "cuda/stream.hpp"
#include "cuda/task_manager.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <random>
#include <vector>

#include <cuda_runtime.h>

namespace velodb {

class ObliviousTable : private NonCopyable {
public:
    ObliviousTable(const std::string& name, Table* source, TaskManager& task_manager);
    ~ObliviousTable();

    // Move support
    ObliviousTable(ObliviousTable&& other) noexcept;
    ObliviousTable& operator=(ObliviousTable&& other) noexcept;

    const std::string& getName() const { return name_; }
    size_t getRowCount() const { return row_count_; }
    const Table* getSourceTable() const { return source_; }

    // ========================================================================
    // Position Map Access
    // ========================================================================

    uint32_t getPosition(uint32_t record_id) const { return h_position_map_[record_id]; }

    uint32_t* getPositionMapDevice() const { return d_position_map_active_; }

    void syncPositionMapToGPU();

    // ========================================================================
    // Async Virtual Shuffle
    // ========================================================================

    void startAsyncShuffle();

    void waitForShuffle();

    bool isShuffling() const { return shuffle_in_progress_.load(); }

    // ========================================================================
    // Statistics
    // ========================================================================

    size_t getShuffleCount() const { return shuffle_count_; }

private:
    std::string name_;
    Table* source_; // Non-owning pointer to actual table data
    TaskManager& task_manager_;
    size_t row_count_;

    // Host position map (for CPU-side access if needed)
    std::vector<uint32_t> h_position_map_;

    // GPU position maps (double-buffered for async shuffle)
    uint32_t* d_position_map_active_ = nullptr; // Current active map
    uint32_t* d_position_map_pending_ = nullptr; // Being written by shuffle
    uint32_t* d_sigma_ = nullptr; // Random permutation buffer

    // Shuffle state
    std::atomic<bool> shuffle_in_progress_ { false };
    uint64_t pending_command_id_ = 0;

    // Synchronization
    mutable std::mutex shuffle_mutex_;

    // RNG for shuffle seeds
    std::mt19937_64 rng_;

    // Stats
    size_t shuffle_count_ = 0;

    // Internal helpers
    void allocateGPUBuffers();
    void freeGPUBuffers();
    std::vector<uint32_t> generatePermutation(size_t n, uint64_t seed);
};

} // namespace velodb
