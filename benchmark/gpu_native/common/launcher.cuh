#pragma once

#include <fmt/core.h>

#include <stdexcept>

#include <cooperative_groups.h>
#include <cuda_runtime.h>

namespace gpu_native {

// ============================================================================
// CUDA Error Checking
// ============================================================================

#define GPU_CHECK(call)                                                                                                \
    do {                                                                                                               \
        cudaError_t err = call;                                                                                        \
        if (err != cudaSuccess) {                                                                                      \
            throw std::runtime_error(                                                                                  \
                fmt::format("CUDA error at {}:{}: {}", __FILE__, __LINE__, cudaGetErrorString(err)));                  \
        }                                                                                                              \
    } while (0)

// ============================================================================
// Cooperative Kernel Launch Helper
// ============================================================================

template <typename KernelFunc, typename... Args>
void launchCooperativeKernel(KernelFunc kernel, cudaStream_t stream, int block_size, size_t shared_mem, Args&&... args)
{
    int device;
    cudaDeviceProp prop;
    int num_blocks_per_sm;

    GPU_CHECK(cudaGetDevice(&device));
    GPU_CHECK(cudaGetDeviceProperties(&prop, device));
    GPU_CHECK(cudaOccupancyMaxActiveBlocksPerMultiprocessor(&num_blocks_per_sm, kernel, block_size, shared_mem));

    // Use all available SMs for maximum parallelism
    int num_blocks = prop.multiProcessorCount * num_blocks_per_sm;

    void* kernel_args[] = { const_cast<void*>(static_cast<const void*>(&args))... };

    GPU_CHECK(cudaLaunchCooperativeKernel(reinterpret_cast<const void*>(kernel),
                                          num_blocks,
                                          block_size,
                                          kernel_args,
                                          shared_mem,
                                          stream));
}

// Simplified version with default parameters
template <typename KernelFunc, typename... Args>
void launchQuery(KernelFunc kernel, cudaStream_t stream, Args&&... args)
{
    launchCooperativeKernel(kernel, stream, 256, 0, std::forward<Args>(args)...);
}

// ============================================================================
// Grid-Stride Loop Helper
// ============================================================================

__device__ __forceinline__ void grid_stride_loop_1d(size_t n, auto&& body)
{
    namespace cg = cooperative_groups;
    cg::grid_group grid = cg::this_grid();

    for (size_t i = grid.thread_rank(); i < n; i += grid.size()) {
        body(i);
    }
}

// ============================================================================
// Output Buffer Management
// ============================================================================

template <typename T>
struct OutputBuffer {
    T* data;
    uint32_t* count;
    size_t capacity;

    __device__ __forceinline__ uint32_t append(const T& value)
    {
        uint32_t pos = atomicAdd(count, 1);
        if (pos < capacity) {
            data[pos] = value;
        }
        return pos;
    }
};

template <typename T>
OutputBuffer<T> allocate_output_buffer(size_t capacity, cudaStream_t stream = 0)
{
    OutputBuffer<T> buf;
    buf.capacity = capacity;
    cudaMalloc(&buf.data, capacity * sizeof(T));
    cudaMalloc(&buf.count, sizeof(uint32_t));
    cudaMemsetAsync(buf.count, 0, sizeof(uint32_t), stream);
    return buf;
}

template <typename T>
void free_output_buffer(OutputBuffer<T>& buf)
{
    cudaFree(buf.data);
    cudaFree(buf.count);
    buf.data = nullptr;
    buf.count = nullptr;
}

// ============================================================================
// Timer for Benchmarking
// ============================================================================

class GpuTimer {
public:
    GpuTimer(cudaStream_t stream = 0)
        : stream_(stream)
    {
        cudaEventCreate(&start_);
        cudaEventCreate(&stop_);
    }

    ~GpuTimer()
    {
        cudaEventDestroy(start_);
        cudaEventDestroy(stop_);
    }

    void start() { cudaEventRecord(start_, stream_); }

    void stop() { cudaEventRecord(stop_, stream_); }

    float elapsed_ms()
    {
        cudaEventSynchronize(stop_);
        float ms;
        cudaEventElapsedTime(&ms, start_, stop_);
        return ms;
    }

private:
    cudaStream_t stream_;
    cudaEvent_t start_;
    cudaEvent_t stop_;
};

} // namespace gpu_native
