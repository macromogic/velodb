#include "cuda/helper.hpp"
#include "cuda/persistent_kernel.hpp"

#include <chrono>
#include <cstring>
#include <thread>

namespace velodb {
namespace cuda {

    CommandQueue::CommandQueue(size_t capacity)
        : capacity_(nextPow2(capacity))
        , capacity_mask_(capacity_ - 1)
    {
        CHECKED_CALL_THROW(cudaMallocManaged(&buffer_, capacity_ * sizeof(Command)));

        cudaMemLocation location;
        location.type = cudaMemLocationTypeDevice;
        CHECKED_CALL_THROW(cudaGetDevice(&location.id));
        CHECKED_CALL_THROW(cudaMemPrefetchAsync(buffer_, capacity_ * sizeof(Command), location, 0));

        for (size_t i = 0; i < capacity_; ++i) {
            buffer_[i] = Command {};
        }
    }

    CommandQueue::~CommandQueue()
    {
        if (buffer_) {
            cudaFree(buffer_);
        }
        buffer_ = nullptr;
    }

    bool CommandQueue::push(const Command& cmd)
    {
        while (full()) {
            // Yield to avoid burning CPU cycles
            std::this_thread::yield();

            // Optional: Add exponential backoff here for production
            // std::this_thread::sleep_for(std::chrono::microseconds(1));
        }

        uint32_t head = head_.load(std::memory_order_relaxed);
        uint32_t next_head = nextIndex(head);

        std::memcpy(&buffer_[head], &cmd, sizeof(Command));

        std::atomic_thread_fence(std::memory_order_release);

        head_.store(next_head, std::memory_order_release);

        return true;
    }

    bool CommandQueue::tryPush(const Command& cmd)
    {
        if (full()) {
            return false;
        }

        uint32_t head = head_.load(std::memory_order_relaxed);
        uint32_t next_head = nextIndex(head);

        std::memcpy(&buffer_[head], &cmd, sizeof(Command));
        std::atomic_thread_fence(std::memory_order_release);

        head_.store(next_head, std::memory_order_release);

        return true;
    }

    void CommandQueue::terminate()
    {
        Command cmd;
        cmd.opcode = OpCode::OP_TERMINATE;
        push(cmd);
    }

    size_t CommandQueue::size() const
    {
        uint32_t head = head_.load(std::memory_order_acquire);
        uint32_t tail = tail_.load(std::memory_order_acquire);

        if (head >= tail) {
            return head - tail;
        } else {
            return capacity_ - (tail - head);
        }
    }

    bool CommandQueue::empty() const
    {
        uint32_t head = head_.load(std::memory_order_acquire);
        uint32_t tail = tail_.load(std::memory_order_acquire);
        return head == tail;
    }

    bool CommandQueue::full() const
    {
        uint32_t head = head_.load(std::memory_order_acquire);
        uint32_t tail = tail_.load(std::memory_order_acquire);
        uint32_t next_head = nextIndex(head);
        return next_head == tail;
    }

    bool CommandQueue::waitUntilDrained(uint32_t timeout_ms)
    {
        auto start = std::chrono::steady_clock::now();

        while (!empty()) {
            if (timeout_ms > 0) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
                if (elapsed.count() >= timeout_ms) {
                    return false;
                }
            }

            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        return true;
    }

    PersistentKernelManager::PersistentKernelManager(size_t queue_capacity, size_t batch_size, uint32_t block_size)
        : queue_(std::make_unique<CommandQueue>(queue_capacity))
        , batch_size_(batch_size)
        , block_size_(block_size)
        , stream_(nullptr)
        , is_running_(false)
    {
    }

    PersistentKernelManager::~PersistentKernelManager()
    {
        if (is_running_) {
            stop(5000);
        }
    }

    bool PersistentKernelManager::start(cudaStream_t stream)
    {
        if (is_running_) {
            return false;
        }

        stream_ = stream;

        // For persistent kernel, we typically launch fewer blocks than work items
        // Let's use a modest number of blocks that can saturate the GPU
        uint32_t num_blocks = 128; // Can be tuned based on H100 SM count (132 SMs)

        Command* cmd_buffer = queue_->getDeviceBuffer();
        uint32_t* head_ptr = reinterpret_cast<uint32_t*>(queue_->getDeviceHead());
        uint32_t* tail_ptr = reinterpret_cast<uint32_t*>(queue_->getDeviceTail());
        uint32_t capacity = static_cast<uint32_t>(queue_->capacity());

        persistentKernel<<<num_blocks, block_size_, 0, stream_>>>(cmd_buffer, head_ptr, tail_ptr, capacity);
        cudaError_t err = cudaGetLastError();
        if (err != cudaSuccess) {
            return false;
        }

        is_running_ = true;
        return true;
    }

    bool PersistentKernelManager::stop(uint32_t timeout_ms)
    {
        if (!is_running_) {
            return true;
        }

        // Send terminate command
        queue_->terminate();

        // Wait for kernel to exit
        auto start = std::chrono::steady_clock::now();

        while (true) {
            cudaError_t err = cudaStreamQuery(stream_);

            if (err == cudaSuccess) {
                // Kernel has completed
                is_running_ = false;
                return true;
            } else if (err != cudaErrorNotReady) {
                // Error occurred
                return false;
            }

            // Check timeout
            if (timeout_ms > 0) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
                if (elapsed.count() >= timeout_ms) {
                    return false;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    __device__ __forceinline__ void dispatchCommand(const Command& cmd)
    {
        switch (cmd.opcode) {
        case OpCode::OP_NOP:
            // Do nothing
            break;

        case OpCode::OP_FILTER:
            // TODO: Implement filter
            break;

        case OpCode::OP_JOIN:
            // TODO: Implement join
            break;

        case OpCode::OP_SORT:
            // TODO: Implement sort
            break;

        case OpCode::OP_TERMINATE:
            // Will be handled in main loop
            break;

        default:
            // Invalid opcode - ignore
            break;
        }
    }

    __global__ void persistentKernel(Command* commands, uint32_t* head_ptr, uint32_t* tail_ptr, uint32_t capacity)
    {
        // Calculate grid dimensions
        const uint32_t capacity_mask = capacity - 1;

        // Local copy of tail (only block 0 thread 0 updates the global tail)
        __shared__ uint32_t local_tail;
        __shared__ bool should_terminate;

        // Initialize shared variables
        if (threadIdx.x == 0 && blockIdx.x == 0) {
            local_tail = *tail_ptr;
            should_terminate = false;
        }

        // Grid-wide synchronization
        __syncthreads();

        // Main polling loop
        while (true) {
            // Only block 0 checks for new commands
            if (blockIdx.x == 0 && threadIdx.x == 0) {
                // Load head pointer with acquire semantics
                uint32_t current_head = *head_ptr;
                __threadfence();

                // Check if there's a new command
                if (local_tail != current_head) {
                    // New command available
                    should_terminate = false;
                } else {
                    // No command available - sleep to reduce power consumption
                    // H100 has improved power management, so nanosleep is efficient
                    __nanosleep(1000); // Sleep for 1 microsecond
                }
            }

            // Synchronize across grid (all blocks wait)
            __syncthreads();

            // Check if we should terminate
            if (should_terminate) {
                break;
            }

            // Process command if available
            uint32_t current_head = *head_ptr;
            __threadfence();

            if (local_tail != current_head) {
                // Load command from queue
                Command cmd = commands[local_tail];
                __threadfence_system(); // Ensure command is fully loaded

                // Check for termination command
                if (cmd.opcode == OpCode::OP_TERMINATE) {
                    should_terminate = true;

                    // Update tail and exit
                    if (blockIdx.x == 0 && threadIdx.x == 0) {
                        local_tail = (local_tail + 1) & capacity_mask;
                        *tail_ptr = local_tail;
                        __threadfence_system();
                    }
                    break;
                }

                // Dispatch command to appropriate operator
                dispatchCommand(cmd);

                // Grid-wide synchronization to ensure all blocks finish
                __syncthreads();

                // Update tail pointer (only one thread does this)
                if (blockIdx.x == 0 && threadIdx.x == 0) {
                    local_tail = (local_tail + 1) & capacity_mask;

                    // Memory fence to ensure all writes are visible
                    __threadfence_system();

                    // Update global tail (makes command slot available to producer)
                    *tail_ptr = local_tail;
                    __threadfence_system();
                }

                // Synchronize before next iteration
                __syncthreads();
            }
        }
    }

} // namespace cuda
} // namespace velodb
