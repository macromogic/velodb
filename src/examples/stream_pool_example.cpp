#include "cuda/stream_pool.hpp"

#include <iostream>

namespace velodb {

/**
 * @brief Example demonstrating StreamPool usage patterns
 */
void stream_pool_examples()
{

    // Example 1: Using StreamGuard (recommended for most cases)
    {
        StreamGuard guard; // Uses global pool by default
        if (guard.is_valid()) {
            cudaStream_t stream = guard->get();

            // Use the stream for CUDA operations
            // cudaMemcpyAsync(..., stream);
            // my_kernel<<<grid, block, 0, stream>>>(...);

            // Stream automatically returned to pool when guard goes out of scope
        }
    }

    // Example 2: Using StreamHandle directly
    {
        auto& pool = StreamPool::instance();
        auto handle_result = pool.acquire_stream();

        if (handle_result) {
            auto handle = std::move(handle_result.value());
            cudaStream_t stream = handle->get();

            // Use the stream...
            // handle.release(); // Optional: release early

            // Stream automatically returned when handle destructor runs
        }
    }

    // Example 3: Custom pool configuration
    {
        StreamPool custom_pool(8, 16); // 8 initial streams, max 16

        auto handle_result = custom_pool.acquire_stream();
        if (handle_result) {
            auto handle = std::move(handle_result.value());

            // Synchronize the stream
            auto sync_result = handle->synchronize();
            if (!sync_result) {
                std::cerr << "Stream sync failed: " << sync_result.error() << std::endl;
            }
        }

        // Synchronize all streams in the pool
        auto sync_all_result = custom_pool.synchronize_all();
        if (sync_all_result) {
            std::cout << "All streams synchronized successfully" << std::endl;
        }
    }

    // Example 4: Pool statistics
    {
        auto& pool = StreamPool::instance();

        std::cout << "Available streams: " << pool.available_count() << std::endl;
        std::cout << "Total streams: " << pool.total_count() << std::endl;
    }

    // Example 5: Error handling pattern
    {
        auto& pool = StreamPool::instance();
        auto handle_result = pool.acquire_stream();

        if (!handle_result) {
            std::cerr << "Failed to acquire stream: " << handle_result.error() << std::endl;
            return;
        }

        auto handle = std::move(handle_result.value());
        if (!handle.is_valid()) {
            std::cerr << "Invalid stream handle" << std::endl;
            return;
        }

        // Use the stream safely...
        cudaStream_t stream = handle->get();

        // Perform CUDA operations...
        auto sync_result = handle->synchronize();
        if (!sync_result) {
            std::cerr << "Synchronization failed: " << sync_result.error() << std::endl;
        }
    }
}

} // namespace velodb
