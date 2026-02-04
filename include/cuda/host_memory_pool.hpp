#pragma once

#include "common/copy_traits.hpp"
#include "cuda/helper.hpp"

#include <backward.hpp>
#include <fmt/ranges.h>

#include <cassert>
#include <map>
#include <mutex>
#include <vector>

namespace velodb {

class HostMemoryPool : private NonCopyable {
public:
    HostMemoryPool(size_t total_size)
        : base_ptr_(nullptr)
        , total_size_(total_size)
    {
        CHECKED_CALL_THROW(cudaMallocHost(&base_ptr_, total_size_));
        free_blocks_[base_ptr_] = total_size;
    }

    ~HostMemoryPool()
    {
        if (base_ptr_) {
            cudaFreeHost(base_ptr_);
        }
    }

    void* allocate(size_t size)
    {
        size = alignUp(size, 256);
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = free_blocks_.begin(); it != free_blocks_.end(); ++it) {
            if (it->second >= size) {
                void* ptr = it->first;
                size_t block_size = it->second;
                free_blocks_.erase(it);

                if (block_size > size) {
                    void* next_ptr = static_cast<char*>(ptr) + size;
                    size_t remaining = block_size - size;
                    free_blocks_[next_ptr] = remaining;
                }

                return ptr;
            }
        }
        VELODB_THROW(ExecutionError,
                     fmt::format("HostMemoryPool: Out of memory. Trying to allocate {} bytes from free blocks {}",
                                 size,
                                 free_blocks_));
    }

    void deallocate(void* ptr, size_t size)
    {
        if (!ptr)
            return;
        size = alignUp(size, 256);

        std::lock_guard<std::mutex> lock(mutex_);
        auto [it, success] = free_blocks_.insert({ ptr, size });
        assert(success && "Double free detected!");

        auto next_it = std::next(it);
        if (next_it != free_blocks_.end()) {
            if (static_cast<char*>(ptr) + size == next_it->first) {
                it->second += next_it->second;
                free_blocks_.erase(next_it);
            }
        }

        if (it != free_blocks_.begin()) {
            auto prev_it = std::prev(it);
            if (static_cast<char*>(prev_it->first) + prev_it->second == ptr) {
                prev_it->second += it->second;
                free_blocks_.erase(it);
            }
        }
    }

    void reset()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        free_blocks_.clear();
        free_blocks_[base_ptr_] = total_size_;
    }

    size_t totalFreeMemory() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t total = 0;
        for (const auto& [ptr, size] : free_blocks_) {
            total += size;
        }
        return total;
    }

    size_t largestFreeBlock() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t largest = 0;
        for (const auto& [ptr, size] : free_blocks_) {
            if (size > largest) {
                largest = size;
            }
        }
        return largest;
    }

    static HostMemoryPool& getInstance()
    {
        // This pinned pool is used for:
        // - Intermediate results during query execution that need pinned memory
        // - Small temporary buffers
        // Main data uses pageable memory (HOST_PAGEABLE) and VIEW data goes
        // directly to CUDA via staged transfer, so pinned memory needs are reduced.
        // Increased to 8GB to support prefetch double-buffering and larger batches.
        constexpr size_t pool_size = 8ul << 30; // 8GB
        static HostMemoryPool instance(pool_size);
        return instance;
    }

private:
    static size_t alignUp(size_t x, size_t align)
    {
        x = std::max(x, 1ul);
        return (x + align - 1) & ~(align - 1);
    }

    mutable std::mutex mutex_;
    char* base_ptr_;
    size_t total_size_;
    std::map<void*, size_t> free_blocks_;
};

}
