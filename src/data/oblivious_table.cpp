#include "data/oblivious_table.hpp"

#include "common/profiler.hpp"
#include "cuda/commands.hpp"
#include "cuda/stream_pool.hpp"

#include <algorithm>
#include <numeric>

namespace velodb {

// ============================================================================
// Constructor / Destructor
// ============================================================================

ObliviousTable::ObliviousTable(const std::string& name, Table* source, TaskManager& task_manager)
    : name_(name)
    , source_(source)
    , task_manager_(task_manager)
    , row_count_(source->getRowCount())
    , rng_(std::random_device {}())
{
    // Initialize position map as identity
    h_position_map_.resize(row_count_);
    std::iota(h_position_map_.begin(), h_position_map_.end(), 0);

    // Perform initial shuffle (CPU-side, before GPU buffers exist)
    auto perm = generatePermutation(row_count_, rng_());
    std::vector<uint32_t> new_map(row_count_);
    for (size_t i = 0; i < row_count_; i++) {
        new_map[i] = h_position_map_[perm[i]];
    }
    h_position_map_ = std::move(new_map);

    // Allocate GPU buffers
    allocateGPUBuffers();

    // Upload initial position map to GPU using a stream
    auto stream_handle = StreamPool::getInstance().acquire().value();
    cudaMemcpyAsync(d_position_map_active_,
                    h_position_map_.data(),
                    row_count_ * sizeof(uint32_t),
                    cudaMemcpyHostToDevice,
                    stream_handle->get());

    // Wait for initialization to complete
    stream_handle->synchronize();
}

ObliviousTable::~ObliviousTable()
{
    // Wait for any pending shuffle
    if (shuffle_in_progress_.load()) {
        waitForShuffle();
    }

    freeGPUBuffers();
}

ObliviousTable::ObliviousTable(ObliviousTable&& other) noexcept
    : name_(std::move(other.name_))
    , source_(other.source_)
    , task_manager_(other.task_manager_)
    , row_count_(other.row_count_)
    , h_position_map_(std::move(other.h_position_map_))
    , d_position_map_active_(other.d_position_map_active_)
    , d_position_map_pending_(other.d_position_map_pending_)
    , d_sigma_(other.d_sigma_)
    , shuffle_in_progress_(other.shuffle_in_progress_.load())
    , pending_command_id_(other.pending_command_id_)
    , rng_(std::move(other.rng_))
    , shuffle_count_(other.shuffle_count_)
{
    other.source_ = nullptr;
    other.row_count_ = 0;
    other.d_position_map_active_ = nullptr;
    other.d_position_map_pending_ = nullptr;
    other.d_sigma_ = nullptr;
}

ObliviousTable& ObliviousTable::operator=(ObliviousTable&& other) noexcept
{
    if (this != &other) {
        // Clean up existing resources
        if (shuffle_in_progress_.load()) {
            waitForShuffle();
        }
        freeGPUBuffers();

        // Move from other
        name_ = std::move(other.name_);
        source_ = other.source_;
        // task_manager_ is a reference, can't reassign
        row_count_ = other.row_count_;
        h_position_map_ = std::move(other.h_position_map_);
        d_position_map_active_ = other.d_position_map_active_;
        d_position_map_pending_ = other.d_position_map_pending_;
        d_sigma_ = other.d_sigma_;
        shuffle_in_progress_.store(other.shuffle_in_progress_.load());
        pending_command_id_ = other.pending_command_id_;
        rng_ = std::move(other.rng_);
        shuffle_count_ = other.shuffle_count_;

        // Invalidate other
        other.source_ = nullptr;
        other.row_count_ = 0;
        other.d_position_map_active_ = nullptr;
        other.d_position_map_pending_ = nullptr;
        other.d_sigma_ = nullptr;
    }
    return *this;
}

// ============================================================================
// GPU Buffer Management
// ============================================================================

void ObliviousTable::allocateGPUBuffers()
{
    size_t map_size = row_count_ * sizeof(uint32_t);

    auto stream_handle = StreamPool::getInstance().acquire().value();
    cudaMallocAsync(&d_position_map_active_, map_size, stream_handle->get());
    cudaMallocAsync(&d_position_map_pending_, map_size, stream_handle->get());
    cudaMallocAsync(&d_sigma_, map_size, stream_handle->get());

    // Initialize pending to zeros
    cudaMemsetAsync(d_position_map_pending_, 0, map_size, stream_handle->get());
    stream_handle->synchronize();
}

void ObliviousTable::freeGPUBuffers()
{
    if (d_position_map_active_) {
        cudaFree(d_position_map_active_);
        d_position_map_active_ = nullptr;
    }
    if (d_position_map_pending_) {
        cudaFree(d_position_map_pending_);
        d_position_map_pending_ = nullptr;
    }
    if (d_sigma_) {
        cudaFree(d_sigma_);
        d_sigma_ = nullptr;
    }
}

// ============================================================================
// Position Map Operations
// ============================================================================

void ObliviousTable::syncPositionMapToGPU()
{
    // If not shuffling, sync from host (in case host map was modified)
    if (!shuffle_in_progress_.load()) {
        auto stream_handle = StreamPool::getInstance().acquire().value();
        cudaMemcpyAsync(d_position_map_active_,
                        h_position_map_.data(),
                        row_count_ * sizeof(uint32_t),
                        cudaMemcpyHostToDevice,
                        stream_handle->get());
        stream_handle->synchronize();
    }
}

// ============================================================================
// Async Shuffle via Command Queue
// ============================================================================

void ObliviousTable::startAsyncShuffle()
{
    std::lock_guard<std::mutex> lock(shuffle_mutex_);

    if (shuffle_in_progress_.load()) {
        // Already shuffling, skip
        return;
    }

    PROFILE_SCOPE("ObliviousTable::startAsyncShuffle");

    // 1. Generate random permutation on CPU
    auto sigma = generatePermutation(row_count_, rng_());

    // 2. Upload sigma to GPU
    auto stream_handle = StreamPool::getInstance().acquire().value();
    cudaMemcpyAsync(d_sigma_,
                    sigma.data(),
                    row_count_ * sizeof(uint32_t),
                    cudaMemcpyHostToDevice,
                    stream_handle->get());
    stream_handle->synchronize();

    // 3. Submit compose command to persistent kernel
    Command cmd = {};
    cmd.opcode = OpCode::OP_COMPOSE_POSITION_MAP;
    cmd.args.compose_position_map
        = { .old_map = d_position_map_active_, .sigma = d_sigma_, .new_map = d_position_map_pending_, .n = row_count_ };

    pending_command_id_ = task_manager_.submitCommand(cmd);
    shuffle_in_progress_.store(true);
    shuffle_count_++;
}

void ObliviousTable::waitForShuffle()
{
    if (!shuffle_in_progress_.load()) {
        return;
    }

    std::lock_guard<std::mutex> lock(shuffle_mutex_);

    if (!shuffle_in_progress_.load()) {
        return; // Double-check after acquiring lock
    }

    PROFILE_SCOPE("ObliviousTable::waitForShuffle");

    // Wait for command to complete
    task_manager_.waitCommand(pending_command_id_);

    // Swap active and pending pointers
    std::swap(d_position_map_active_, d_position_map_pending_);

    // Update host copy
    auto stream_handle = StreamPool::getInstance().acquire().value();
    cudaMemcpyAsync(h_position_map_.data(),
                    d_position_map_active_,
                    row_count_ * sizeof(uint32_t),
                    cudaMemcpyDeviceToHost,
                    stream_handle->get());
    stream_handle->synchronize();

    shuffle_in_progress_.store(false);
}

// ============================================================================
// Helpers
// ============================================================================

std::vector<uint32_t> ObliviousTable::generatePermutation(size_t n, uint64_t seed)
{
    PROFILE_SCOPE("generatePermutation");

    std::vector<uint32_t> perm(n);
    std::iota(perm.begin(), perm.end(), 0);

    std::mt19937_64 rng(seed);

    // Fisher-Yates shuffle
    for (size_t i = n - 1; i > 0; i--) {
        std::uniform_int_distribution<size_t> dist(0, i);
        size_t j = dist(rng);
        std::swap(perm[i], perm[j]);
    }

    return perm;
}

} // namespace velodb
