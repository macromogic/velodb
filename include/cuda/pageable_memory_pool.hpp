#pragma once

#include "common/copy_traits.hpp"

#include <cstdlib>
#include <mutex>

namespace velodb {

/**
 * @brief A simple memory pool for pageable (regular) host memory.
 *
 * This class provides a unified interface for allocating and deallocating
 * regular host memory. Unlike HostMemoryPool which uses cudaMallocHost for
 * pinned memory, this uses standard malloc/free.
 *
 * The main advantages of pageable memory:
 * - No limit on total allocation size (unlike pinned memory)
 * - Works well in memory-constrained environments (TDX+CC)
 * - Can be swapped to disk if needed
 *
 * The main disadvantage:
 * - Cannot be used directly for DMA transfers; requires staging through pinned memory
 */
class PageableMemoryPool : private NonCopyable {
public:
    PageableMemoryPool() = default;
    ~PageableMemoryPool() = default;

    /**
     * @brief Allocate memory from the pool.
     * @param size Number of bytes to allocate
     * @return Pointer to allocated memory, or nullptr on failure
     */
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

    /**
     * @brief Deallocate memory back to the pool.
     * @param ptr Pointer to memory to deallocate
     * @param size Size of the allocation (for tracking purposes)
     */
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

    /**
     * @brief Get the current amount of allocated memory.
     */
    size_t currentAllocated() const
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return current_allocated_;
    }

    /**
     * @brief Get the peak amount of allocated memory.
     */
    size_t peakAllocated() const
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return peak_allocated_;
    }

    /**
     * @brief Get the total amount of memory allocated over the lifetime of the pool.
     */
    size_t totalAllocated() const
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return total_allocated_;
    }

    /**
     * @brief Get the singleton instance.
     */
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
