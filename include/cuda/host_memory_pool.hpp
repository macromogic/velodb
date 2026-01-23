#pragma once

#include "common/copy_traits.hpp"
#include "cuda/helper.hpp"

#include <backward.hpp>

#include <cassert>
#include <iostream>
#include <map>
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
        for (auto it = free_blocks_.begin(); it != free_blocks_.end(); ++it) {
            if (it->second >= size) {
                char* ptr = it->first;
                size_t block_size = it->second;
                free_blocks_.erase(it);

                if (block_size > size) {
                    char* next_ptr = ptr + size;
                    size_t remaining = block_size - size;
                    free_blocks_[next_ptr] = remaining;
                }

                // fmt::println("HostMemoryPool: Allocated {} bytes at {}", size, (void*)ptr);
                // backward::StackTrace st;
                // st.load_here(8);
                // backward::Printer printer;
                // printer.snippet = false;
                // printer.print(st);

                return ptr;
            }
        }
        VELODB_THROW(ExecutionError, "HostMemoryPool: Out of memory");
    }

    void deallocate(void* ptr, size_t size)
    {
        if (!ptr)
            return;
        char* char_ptr = static_cast<char*>(ptr);
        size = alignUp(size, 256);

        // fmt::println("HostMemoryPool: Deallocated {} bytes at {}", size, (void*)char_ptr);
        // backward::StackTrace st;
        // st.load_here(8);
        // backward::Printer printer;
        // printer.snippet = false;
        // printer.print(st);

        auto [it, success] = free_blocks_.insert({ char_ptr, size });
        assert(success && "Double free detected!");

        auto next_it = std::next(it);
        if (next_it != free_blocks_.end()) {
            if (char_ptr + size == next_it->first) {
                it->second += next_it->second;
                free_blocks_.erase(next_it);
            }
        }

        if (it != free_blocks_.begin()) {
            auto prev_it = std::prev(it);
            if (prev_it->first + prev_it->second == char_ptr) {
                prev_it->second += it->second;
                free_blocks_.erase(it);
            }
        }
    }

    void debug() const
    {
        std::cout << "--- Pool Status ---\n";
        size_t free_total = 0;
        for (auto const& [ptr, size] : free_blocks_) {
            std::cout << "Free Block: " << (void*)ptr << " | Size: " << size << "\n";
            free_total += size;
        }
        std::cout << "Total Free: " << free_total << " / " << total_size_ << "\n";
        std::cout << "-------------------\n";
    }

    static HostMemoryPool& getInstance()
    {
        constexpr size_t pool_size = 1ul << 26; // 64 MB
        static HostMemoryPool instance(pool_size);
        return instance;
    }

private:
    static size_t alignUp(size_t x, size_t align)
    {
        x = std::max(x, 1ul);
        return (x + align - 1) & ~(align - 1);
    }

    char* base_ptr_;
    size_t total_size_;
    std::map<char*, size_t> free_blocks_;
};

}
