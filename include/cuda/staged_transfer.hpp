#pragma once

#include "common/exception.hpp"
#include "common/profiler.hpp"
#include "cuda/helper.hpp"
#include "cuda/staging_buffer_pool.hpp"

#include <algorithm>
#include <cstring>

#include <cuda_runtime.h>

namespace velodb {

class StagedTransfer {
public:
    static void toDevice(void* device_dst, const void* pageable_src, size_t total_bytes, cudaStream_t stream = nullptr)
    {
        PROFILE_SCOPE("StagedTransfer::toDevice");

        if (total_bytes == 0) {
            return;
        }

        auto& pool = StagingBufferPool::getInstance();
        StagingBufferGuard guard(pool);

        if (!guard) {
            VELODB_THROW(ExecutionError, "Failed to acquire staging buffer");
        }

        auto* staging = guard.get();
        const size_t chunk_size = staging->size;
        cudaStream_t transfer_stream = stream ? stream : staging->stream;

        const char* src = static_cast<const char*>(pageable_src);
        char* dst = static_cast<char*>(device_dst);

        size_t remaining = total_bytes;
        while (remaining > 0) {
            size_t transfer_size = std::min(remaining, chunk_size);

            // Copy from pageable to staging (CPU memcpy)
            std::memcpy(staging->ptr, src, transfer_size);

            // Copy from staging to device (async DMA)
            CHECKED_CALL_THROW(
                cudaMemcpyAsync(dst, staging->ptr, transfer_size, cudaMemcpyHostToDevice, transfer_stream));

            // Wait for this chunk to complete before reusing staging buffer
            CHECKED_CALL_THROW(cudaStreamSynchronize(transfer_stream));

            src += transfer_size;
            dst += transfer_size;
            remaining -= transfer_size;
        }
    }

    static void toHost(void* pageable_dst, const void* device_src, size_t total_bytes, cudaStream_t stream = nullptr)
    {
        PROFILE_SCOPE("StagedTransfer::toHost");

        if (total_bytes == 0) {
            return;
        }

        auto& pool = StagingBufferPool::getInstance();
        StagingBufferGuard guard(pool);

        if (!guard) {
            VELODB_THROW(ExecutionError, "Failed to acquire staging buffer");
        }

        auto* staging = guard.get();
        const size_t chunk_size = staging->size;
        cudaStream_t transfer_stream = stream ? stream : staging->stream;

        const char* src = static_cast<const char*>(device_src);
        char* dst = static_cast<char*>(pageable_dst);

        size_t remaining = total_bytes;
        while (remaining > 0) {
            size_t transfer_size = std::min(remaining, chunk_size);

            // Copy from device to staging (async DMA)
            CHECKED_CALL_THROW(
                cudaMemcpyAsync(staging->ptr, src, transfer_size, cudaMemcpyDeviceToHost, transfer_stream));

            // Wait for DMA to complete
            CHECKED_CALL_THROW(cudaStreamSynchronize(transfer_stream));

            // Copy from staging to pageable (CPU memcpy)
            std::memcpy(dst, staging->ptr, transfer_size);

            src += transfer_size;
            dst += transfer_size;
            remaining -= transfer_size;
        }
    }

    static void toDevicePipelined(void* device_dst, const void* pageable_src, size_t total_bytes)
    {
        PROFILE_SCOPE("H2D Transfer: StagedTransfer");

        if (total_bytes == 0) {
            return;
        }

        auto& pool = StagingBufferPool::getInstance();

        // Try to acquire two buffers for double buffering
        StagingBufferGuard guard1(pool);
        auto maybe_guard2 = pool.tryAcquire();

        if (!guard1) {
            VELODB_THROW(ExecutionError, "Failed to acquire staging buffer");
        }

        // If we couldn't get two buffers, fall back to single buffer
        if (!maybe_guard2.has_value()) {
            toDevice(device_dst, pageable_src, total_bytes);
            return;
        }

        auto* staging1 = guard1.get();
        auto* staging2 = maybe_guard2.value();

        const size_t chunk_size = staging1->size;
        const char* src = static_cast<const char*>(pageable_src);
        char* dst = static_cast<char*>(device_dst);

        size_t remaining = total_bytes;
        int current_buffer = 0;
        StagingBufferPool::StagingBuffer* buffers[2] = { staging1, staging2 };

        // Initial fill of first buffer
        size_t transfer_size = std::min(remaining, chunk_size);
        std::memcpy(buffers[0]->ptr, src, transfer_size);

        while (remaining > 0) {
            auto* current = buffers[current_buffer];
            auto* next = buffers[1 - current_buffer];

            size_t current_size = std::min(remaining, chunk_size);

            // Start async transfer for current buffer
            CHECKED_CALL_THROW(
                cudaMemcpyAsync(dst, current->ptr, current_size, cudaMemcpyHostToDevice, current->stream));

            src += current_size;
            dst += current_size;
            remaining -= current_size;

            // While DMA is in progress, fill the next buffer (if there's more data)
            if (remaining > 0) {
                size_t next_size = std::min(remaining, chunk_size);
                std::memcpy(next->ptr, src, next_size);
            }

            // Wait for current transfer to complete
            CHECKED_CALL_THROW(cudaStreamSynchronize(current->stream));

            current_buffer = 1 - current_buffer;
        }

        pool.release(staging2);
    }

    static void toHostPipelined(void* pageable_dst, const void* device_src, size_t total_bytes)
    {
        PROFILE_SCOPE("D2H Transfer: StagedTransfer");

        if (total_bytes == 0) {
            return;
        }

        auto& pool = StagingBufferPool::getInstance();

        StagingBufferGuard guard1(pool);
        auto maybe_guard2 = pool.tryAcquire();

        if (!guard1) {
            VELODB_THROW(ExecutionError, "Failed to acquire staging buffer");
        }

        if (!maybe_guard2.has_value()) {
            toHost(pageable_dst, device_src, total_bytes);
            return;
        }

        auto* staging1 = guard1.get();
        auto* staging2 = maybe_guard2.value();

        const size_t chunk_size = staging1->size;
        const char* src = static_cast<const char*>(device_src);
        char* dst = static_cast<char*>(pageable_dst);

        size_t remaining = total_bytes;
        int current_buffer = 0;
        StagingBufferPool::StagingBuffer* buffers[2] = { staging1, staging2 };

        while (remaining > 0) {
            auto* current = buffers[current_buffer];

            size_t current_size = std::min(remaining, chunk_size);

            // Start async transfer from device to staging
            CHECKED_CALL_THROW(
                cudaMemcpyAsync(current->ptr, src, current_size, cudaMemcpyDeviceToHost, current->stream));

            // Wait for transfer to complete
            CHECKED_CALL_THROW(cudaStreamSynchronize(current->stream));

            // Copy from staging to pageable (could overlap with next DMA if we had more buffers)
            std::memcpy(dst, current->ptr, current_size);

            src += current_size;
            dst += current_size;
            remaining -= current_size;

            current_buffer = 1 - current_buffer;
        }

        pool.release(staging2);
    }
};

} // namespace velodb
