#pragma once

#include "common/result.hpp"

#include <memory>
#include <mutex>
#include <queue>

#include <cuda_runtime.h>

namespace velodb {

class CudaStream;

class StreamPool {
public:
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

        bool isValid() const { return stream_ != nullptr; }

        void release();

    private:
        StreamPool* pool_;
        std::unique_ptr<CudaStream> stream_;
    };

    explicit StreamPool(size_t initial_size = 4, size_t max_size = 16);
    ~StreamPool();

    // Non-copyable and non-movable (singleton-like resource manager)
    StreamPool(const StreamPool&) = delete;
    StreamPool& operator=(const StreamPool&) = delete;
    StreamPool(StreamPool&&) = delete;
    StreamPool& operator=(StreamPool&&) = delete;

    Result<StreamHandle> acquire();
    size_t availableCount() const;
    size_t totalCount() const;
    Result<void> synchronizeAll();
    void clear();

    static StreamPool& getInstance();

private:
    friend class StreamHandle;

    void returnStream(std::unique_ptr<CudaStream> stream);
    Result<std::unique_ptr<CudaStream>> createStream();

    mutable std::mutex mutex_;
    std::queue<std::unique_ptr<CudaStream>> available_streams_;
    size_t total_count_;
    size_t max_size_;
    bool destroyed_;
};

class StreamGuard {
public:
    explicit StreamGuard(StreamPool& pool = StreamPool::getInstance());
    ~StreamGuard();

    // Non-copyable but movable
    StreamGuard(const StreamGuard&) = delete;
    StreamGuard& operator=(const StreamGuard&) = delete;
    StreamGuard(StreamGuard&& other) noexcept;
    StreamGuard& operator=(StreamGuard&& other) noexcept;

    CudaStream* get() const;
    CudaStream* operator->() const;
    CudaStream& operator*() const;

    bool isValid() const;

    void release();

private:
    StreamPool::StreamHandle handle_;
};

} // namespace velodb
