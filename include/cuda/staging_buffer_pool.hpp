#pragma once

#include "common/copy_traits.hpp"
#include "common/exception.hpp"
#include "cuda/helper.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <vector>

#include <cuda_runtime.h>

namespace velodb {

/**
 * @brief A pool of fixed-size pinned memory buffers for staging data transfers.
 *
 * This class provides a small set of reusable pinned memory buffers that can be used
 * as intermediate staging areas for transfers between pageable host memory and device memory.
 * This allows the main data to reside in regular (pageable) memory while only using a small
 * amount of pinned memory for actual DMA transfers.
 *
 * Key features:
 * - Fixed number of fixed-size buffers allocated at startup
 * - Thread-safe acquire/release with blocking wait when all buffers are in use
 * - Designed to work with persistent kernels (no cudaFreeHost during operation)
 */
class StagingBufferPool : private NonCopyable {
public:
    struct StagingBuffer {
        void* ptr; // Pinned memory pointer
        size_t size; // Buffer size in bytes
        cudaStream_t stream; // Associated CUDA stream for this buffer
        size_t index; // Index in the pool (for identification)

        StagingBuffer(void* p, size_t s, cudaStream_t str, size_t idx)
            : ptr(p)
            , size(s)
            , stream(str)
            , index(idx)
        {
        }
    };

    /**
     * @brief Construct a staging buffer pool.
     * @param buffer_size Size of each staging buffer (default: 64MB)
     * @param num_buffers Number of buffers in the pool (default: 4)
     */
    explicit StagingBufferPool(size_t buffer_size = 64 * 1024 * 1024, size_t num_buffers = 4)
        : buffer_size_(buffer_size)
        , num_buffers_(num_buffers)
        , shutdown_(false)
    {
        buffers_.reserve(num_buffers_);
        in_use_.resize(num_buffers_, false);

        for (size_t i = 0; i < num_buffers_; ++i) {
            void* ptr = nullptr;
            cudaStream_t stream = nullptr;

            CHECKED_CALL_THROW(cudaMallocHost(&ptr, buffer_size_));
            CHECKED_CALL_THROW(cudaStreamCreate(&stream));

            buffers_.emplace_back(ptr, buffer_size_, stream, i);
        }
    }

    ~StagingBufferPool()
    {
        shutdown_ = true;
        cv_.notify_all();

        // Wait for all buffers to be released
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]() {
                for (bool used : in_use_) {
                    if (used)
                        return false;
                }
                return true;
            });
        }

        // Now safe to free (assuming persistent kernel has ended or we're shutting down)
        for (auto& buffer : buffers_) {
            if (buffer.stream) {
                cudaStreamSynchronize(buffer.stream);
                cudaStreamDestroy(buffer.stream);
            }
            if (buffer.ptr) {
                cudaFreeHost(buffer.ptr);
            }
        }
    }

    /**
     * @brief Acquire a staging buffer from the pool.
     * Blocks if no buffer is available.
     * @return Pointer to a StagingBuffer, or nullopt if pool is shutting down
     */
    std::optional<StagingBuffer*> acquire()
    {
        std::unique_lock<std::mutex> lock(mutex_);

        cv_.wait(lock, [this]() {
            if (shutdown_)
                return true;
            for (bool used : in_use_) {
                if (!used)
                    return true;
            }
            return false;
        });

        if (shutdown_) {
            return std::nullopt;
        }

        for (size_t i = 0; i < num_buffers_; ++i) {
            if (!in_use_[i]) {
                in_use_[i] = true;
                return &buffers_[i];
            }
        }

        return std::nullopt; // Should not reach here
    }

    /**
     * @brief Try to acquire a staging buffer without blocking.
     * @return Pointer to a StagingBuffer, or nullopt if none available
     */
    std::optional<StagingBuffer*> tryAcquire()
    {
        std::unique_lock<std::mutex> lock(mutex_);

        for (size_t i = 0; i < num_buffers_; ++i) {
            if (!in_use_[i]) {
                in_use_[i] = true;
                return &buffers_[i];
            }
        }

        return std::nullopt;
    }

    /**
     * @brief Release a staging buffer back to the pool.
     * @param buffer The buffer to release
     */
    void release(StagingBuffer* buffer)
    {
        if (!buffer)
            return;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            VELODB_ASSERT_MSG(buffer->index < num_buffers_, "Invalid buffer index");
            VELODB_ASSERT_MSG(in_use_[buffer->index], "Buffer was not in use");
            in_use_[buffer->index] = false;
        }
        cv_.notify_one();
    }

    /**
     * @brief Get the size of each staging buffer.
     */
    size_t bufferSize() const { return buffer_size_; }

    /**
     * @brief Get the number of buffers in the pool.
     */
    size_t numBuffers() const { return num_buffers_; }

    /**
     * @brief Get the singleton instance of the staging buffer pool.
     */
    static StagingBufferPool& getInstance()
    {
        // Default: 4 buffers of 64MB each = 256MB total pinned memory
        static StagingBufferPool instance(64 * 1024 * 1024, 4);
        return instance;
    }

private:
    size_t buffer_size_;
    size_t num_buffers_;
    std::vector<StagingBuffer> buffers_;
    std::vector<bool> in_use_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> shutdown_;
};

/**
 * @brief RAII wrapper for staging buffer acquisition.
 */
class StagingBufferGuard : private NonCopyable {
public:
    explicit StagingBufferGuard(StagingBufferPool& pool)
        : pool_(pool)
        , buffer_(nullptr)
    {
        auto result = pool_.acquire();
        if (result.has_value()) {
            buffer_ = result.value();
        }
    }

    ~StagingBufferGuard()
    {
        if (buffer_) {
            pool_.release(buffer_);
        }
    }

    StagingBufferGuard(StagingBufferGuard&& other) noexcept
        : pool_(other.pool_)
        , buffer_(other.buffer_)
    {
        other.buffer_ = nullptr;
    }

    StagingBufferPool::StagingBuffer* get() { return buffer_; }
    const StagingBufferPool::StagingBuffer* get() const { return buffer_; }

    StagingBufferPool::StagingBuffer* operator->() { return buffer_; }
    const StagingBufferPool::StagingBuffer* operator->() const { return buffer_; }

    explicit operator bool() const { return buffer_ != nullptr; }

private:
    StagingBufferPool& pool_;
    StagingBufferPool::StagingBuffer* buffer_;
};

} // namespace velodb
