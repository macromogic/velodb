#pragma once

#include "common/copy_traits.hpp"

#include <cstdlib>
#include <mutex>

namespace velodb {

class PageableMemoryPool : private NonCopyable {
public:
    PageableMemoryPool() = default;
    ~PageableMemoryPool() = default;

    void* allocate(size_t size)
    {
        if (size == 0) {
            return nullptr;
        }

        // Align to 256 bytes for better performance
        size = alignUp(size, 256);

        void* ptr = std::aligned_alloc(256, size);
        if (ptr) {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            total_allocated_ += size;
            current_allocated_ += size;
            if (current_allocated_ > peak_allocated_) {
                peak_allocated_ = current_allocated_;
            }
        }
        return ptr;
    }

    void deallocate(void* ptr, size_t size)
    {
        if (!ptr) {
            return;
        }

        size = alignUp(size, 256);

        std::free(ptr);

        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            current_allocated_ -= size;
            total_deallocated_ += size;
        }
    }

    size_t currentAllocated() const
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return current_allocated_;
    }

    size_t peakAllocated() const
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return peak_allocated_;
    }

    size_t totalAllocated() const
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return total_allocated_;
    }

    static PageableMemoryPool& getInstance()
    {
        static PageableMemoryPool instance;
        return instance;
    }

private:
    static size_t alignUp(size_t x, size_t align)
    {
        x = std::max(x, size_t(1));
        return (x + align - 1) & ~(align - 1);
    }

    mutable std::mutex stats_mutex_;
    size_t total_allocated_ = 0;
    size_t total_deallocated_ = 0;
    size_t current_allocated_ = 0;
    size_t peak_allocated_ = 0;
};

} // namespace velodb
