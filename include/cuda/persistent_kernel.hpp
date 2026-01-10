#pragma once

#include "common/copy_traits.hpp"

#include <atomic>
#include <cstddef>
#include <memory>

#include <cuda_runtime.h>

namespace velodb {
namespace cuda {

    enum class OpCode : uint32_t {
        OP_NOP = 0,
        OP_FILTER,
        OP_JOIN,
        OP_SORT,
        OP_TERMINATE = 0xFFFFFFFF
    };

    union alignas(16) CommandArgs {
        struct {
            void* input_data;
            void* output_indices;
            void* predicate_column;
            int64_t comparison_value;
            uint32_t comparison_op;
            uint32_t input_size;
            uint32_t* output_size;
        } filter;

        struct {
            void* left_data;
            void* right_data;
            void* left_keys;
            void* right_keys;
            void* output_buffer;
            uint32_t left_size;
            uint32_t right_size;
            uint32_t* output_size;
        } join;

        // Raw pointer array for maximum flexibility (8 pointers)
        void* ptrs[8];

        // Raw uint64 array for scalar values
        uint64_t values[8];
    };

    struct alignas(64) Command {
        OpCode opcode;
        CommandArgs args;

        Command()
            : opcode(OpCode::OP_NOP)
            , args {}
        {
        }
    };

    class CommandQueue : private NonCopyable {
    public:
        explicit CommandQueue(size_t capacity = 1024);
        ~CommandQueue();

        bool push(const Command& cmd);
        bool tryPush(const Command& cmd);
        void terminate();
        Command* getDeviceBuffer() const { return buffer_; }
        std::atomic<uint32_t>* getDeviceHead() { return &head_; }
        std::atomic<uint32_t>* getDeviceTail() { return &tail_; }
        size_t capacity() const { return capacity_; }
        size_t size() const;
        bool empty() const;
        bool full() const;
        bool waitUntilDrained(uint32_t timeout_ms = 0);

    private:
        Command* buffer_;
        size_t capacity_;
        size_t capacity_mask_; // capacity - 1 for wraparound

        alignas(64) std::atomic<uint32_t> head_ { 0 };
        alignas(64) std::atomic<uint32_t> tail_ { 0 };

        bool use_unified_memory_;

        uint32_t nextIndex(uint32_t current) const { return (current + 1) & capacity_mask_; }
    };

    class PersistentKernelManager : private NonCopyable {
    public:
        explicit PersistentKernelManager(size_t queue_capacity = 1024,
                                         size_t batch_size = 1048576,
                                         uint32_t block_size = 256);

        ~PersistentKernelManager();

        bool start(cudaStream_t stream = nullptr);
        bool stop(uint32_t timeout_ms = 5000);
        bool isRunning() const { return is_running_; }
        CommandQueue& getQueue() { return *queue_; }
        size_t getBatchSize() const { return batch_size_; }
        uint32_t getBlockSize() const { return block_size_; }

    private:
        std::unique_ptr<CommandQueue> queue_;
        size_t batch_size_;
        uint32_t block_size_;
        cudaStream_t stream_;
        bool is_running_;
        bool use_unified_memory_;
    };

    __global__ void persistentKernel(Command* commands, uint32_t* head, uint32_t* tail, uint32_t capacity);

} // namespace cuda
} // namespace velodb
