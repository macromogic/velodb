#pragma once

#include "common/result.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

#include <cuda_runtime.h>

namespace velodb {

class CudaStream; // Forward declaration

/**
 * @brief Thread-safe CUDA stream pool for managing reusable streams
 *
 * The StreamPool manages a pool of CUDA streams to avoid the overhead
 * of creating and destroying streams repeatedly. It provides thread-safe
 * access to streams and automatically handles stream lifecycle.
 */
class StreamPool {
public:
    /**
     * @brief RAII handle for a stream from the pool
     *
     * This handle automatically returns the stream to the pool when destroyed.
     * Follows VeloDB's move-only semantics for clear ownership transfer.
     */
    class StreamHandle {
    public:
        StreamHandle()
            : pool_(nullptr)
            , stream_(nullptr)
        {
        }
        StreamHandle(StreamPool* pool, std::unique_ptr<CudaStream> stream);
        ~StreamHandle();

        // Non-copyable but movable
        StreamHandle(const StreamHandle&) = delete;
        StreamHandle& operator=(const StreamHandle&) = delete;
        StreamHandle(StreamHandle&& other) noexcept;
        StreamHandle& operator=(StreamHandle&& other) noexcept;

        CudaStream* get() const { return stream_.get(); }
        CudaStream* operator->() const { return stream_.get(); }
        CudaStream& operator*() const { return *stream_; }

        bool is_valid() const { return stream_ != nullptr; }

        /**
         * @brief Release the stream early (before destructor)
         */
        void release();

    private:
        StreamPool* pool_;
        std::unique_ptr<CudaStream> stream_;
    };

    /**
     * @brief Construct a new Stream Pool
     * @param initial_size Initial number of streams to create
     * @param max_size Maximum number of streams in the pool (0 = unlimited)
     */
    explicit StreamPool(size_t initial_size = 4, size_t max_size = 16);
    ~StreamPool();

    // Non-copyable and non-movable (singleton-like resource manager)
    StreamPool(const StreamPool&) = delete;
    StreamPool& operator=(const StreamPool&) = delete;
    StreamPool(StreamPool&&) = delete;
    StreamPool& operator=(StreamPool&&) = delete;

    /**
     * @brief Get a stream from the pool
     * @return Result containing a StreamHandle, or error
     */
    Result<StreamHandle> acquire_stream();

    /**
     * @brief Get the number of available streams in the pool
     */
    size_t available_count() const;

    /**
     * @brief Get the total number of streams (available + in-use)
     */
    size_t total_count() const;

    /**
     * @brief Synchronize all streams in the pool
     * @return Result indicating success or failure
     */
    Result<void> synchronize_all();

    /**
     * @brief Clear the pool and destroy all streams
     */
    void clear();

    /**
     * @brief Get the global stream pool instance
     * @return Reference to the singleton stream pool
     */
    static StreamPool& instance();

private:
    friend class StreamHandle;

    /**
     * @brief Return a stream to the pool for reuse (called by StreamHandle)
     * @param stream The stream to return
     */
    void return_stream(std::unique_ptr<CudaStream> stream);

    /**
     * @brief Create a new stream
     * @return Result containing a unique pointer to the new stream
     */
    Result<std::unique_ptr<CudaStream>> create_stream();

    mutable std::mutex mutex_;
    std::queue<std::unique_ptr<CudaStream>> available_streams_;
    size_t total_count_;
    size_t max_size_;
    bool destroyed_;
};

/**
 * @brief Convenience RAII guard for automatic stream management
 *
 * Alternative to using StreamHandle directly. Automatically acquires
 * a stream from the default pool and releases it on destruction.
 */
class StreamGuard {
public:
    explicit StreamGuard(StreamPool& pool = StreamPool::instance());
    ~StreamGuard();

    // Non-copyable but movable
    StreamGuard(const StreamGuard&) = delete;
    StreamGuard& operator=(const StreamGuard&) = delete;
    StreamGuard(StreamGuard&& other) noexcept;
    StreamGuard& operator=(StreamGuard&& other) noexcept;

    CudaStream* get() const;
    CudaStream* operator->() const;
    CudaStream& operator*() const;

    bool is_valid() const;

    /**
     * @brief Release the stream early (before destructor)
     */
    void release();

private:
    StreamPool::StreamHandle handle_;
};

} // namespace velodb
