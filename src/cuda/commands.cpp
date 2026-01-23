#include "cuda/commands.hpp"

#include "cuda/helper.hpp"

#include <thread>

#include <immintrin.h>

namespace velodb {

CommandQueueController::CommandQueueController(uint32_t capacity)
    : queue_ {}
    , cached_tail_(0)
    , current_id_(0)
    , stream_(nullptr)
{
    queue_.capacity = capacity;
    queue_.capacity_mask = capacity - 1;
    CHECKED_CALL_THROW(cudaHostAlloc(&queue_.ring_buffer, capacity * sizeof(Command), cudaHostAllocMapped));
    CHECKED_CALL_THROW(cudaHostAlloc(&queue_.host_ctrl, sizeof(HostControl), cudaHostAllocMapped));
    CHECKED_CALL_THROW(cudaMalloc(&queue_.device_status, sizeof(DeviceStatus)));
    for (uint32_t i = 0; i < capacity; ++i) {
        new (&queue_.ring_buffer[i]) Command {};
    }
    queue_.host_ctrl->head = 0;
    queue_.host_ctrl->tail = 0;
    CHECKED_CALL_THROW(cudaMemset(queue_.device_status, 0, sizeof(DeviceStatus)));
    CHECKED_CALL_THROW(cudaStreamCreate(&stream_));
}

CommandQueueController::~CommandQueueController()
{
    if (queue_.ring_buffer) {
        cudaFreeHost(queue_.ring_buffer);
        queue_.ring_buffer = nullptr;
    }
    if (queue_.host_ctrl) {
        cudaFreeHost(queue_.host_ctrl);
        queue_.host_ctrl = nullptr;
    }
    if (queue_.device_status) {
        cudaFree(queue_.device_status);
        queue_.device_status = nullptr;
    }
}

uint64_t CommandQueueController::push(Command& cmd)
{
    uint32_t head = queue_.host_ctrl->head;
    uint32_t next_head = (head + 1) & queue_.capacity_mask;

    // Wait if the queue is full
    if (next_head == cached_tail_) {
        cached_tail_ = queue_.host_ctrl->tail;
        while (next_head == cached_tail_) {
            _mm_pause();
            cached_tail_ = queue_.host_ctrl->tail;
        }
    }

    current_id_++;
    cmd.sequence_id = current_id_;
    queue_.ring_buffer[head] = cmd;
    std::atomic_thread_fence(std::memory_order_release);
    queue_.host_ctrl->head = next_head;
    return current_id_;
}

void CommandQueueController::wait(uint64_t wait_id)
{
    constexpr int MAX_SPINS = 2000;
    if (queue_.host_ctrl->last_finished_id >= wait_id) {
        std::atomic_thread_fence(std::memory_order_acquire);
        return;
    }

    int spin_count = 0;
    while (queue_.host_ctrl->last_finished_id < wait_id) {
        cudaError_t err = cudaStreamQuery(stream_);
        if (err != cudaSuccess && err != cudaErrorNotReady) {
            VELODB_THROW(ExecutionError, fmt::format("CUDA runtime error: {}", cudaGetErrorString(err)));
        }
        if (spin_count < MAX_SPINS) {
            _mm_pause();
            spin_count++;
        } else {
            std::this_thread::yield();
        }
    }
    std::atomic_thread_fence(std::memory_order_acquire);
}

} // namespace velodb
