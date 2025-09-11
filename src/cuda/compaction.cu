#include "cuda/compaction.hpp"
#include "cuda/helper.hpp"
#include "data/type_traits.hpp"

#include <cstdint>

#include <cuda_runtime.h>

namespace velodb {

namespace gpu {

    __device__ __forceinline__ unsigned int lane_id()
    {
        return threadIdx.x & 31u;
    }

    __device__ __forceinline__ int warp_exclusive_prefix(unsigned int ballot, unsigned int lane)
    {
        unsigned int lower = ballot & ((1u << lane) - 1u);
        return __popc(lower);
    }

    __global__ void countMaskPerBlock(const uint8_t* __restrict__ mask,
                                      size_t n,
                                      unsigned int* __restrict__ block_counts)
    {
        extern __shared__ unsigned int ssum[]; // blockDim.x elements
        const unsigned int gid = blockIdx.x * blockDim.x + threadIdx.x;
        const unsigned int stride = gridDim.x * blockDim.x;

        unsigned int local = 0;
        for (size_t i = gid; i < n; i += stride) {
            local += (mask[i] != 0);
        }

        ssum[threadIdx.x] = local;
        __syncthreads();

        for (unsigned int offset = blockDim.x >> 1; offset > 0; offset >>= 1) {
            if (threadIdx.x < offset) {
                ssum[threadIdx.x] += ssum[threadIdx.x + offset];
            }
            __syncthreads();
        }

        if (threadIdx.x == 0) {
            block_counts[blockIdx.x] = ssum[0];
        }
    }

    template <typename T>
    __global__ void filterCompactionKernel(T* __restrict__ dst,
                                           const T* __restrict__ src,
                                           const uint8_t* __restrict__ mask,
                                           size_t n,
                                           const unsigned int* __restrict__ block_offsets)
    {
        constexpr unsigned int WARP = 32;
        const unsigned int tid = threadIdx.x;
        const unsigned int gid = blockIdx.x * blockDim.x + threadIdx.x;
        const unsigned int lane = lane_id();
        const unsigned int warpIdInBlock = tid / WARP;
        const unsigned int numWarps = (blockDim.x + WARP - 1) / WARP;

        extern __shared__ unsigned int shmem[];
        unsigned int* warp_counts = shmem; // [numWarps]
        unsigned int* warp_prefix = shmem + numWarps; // [numWarps]
        __shared__ unsigned int block_total;

        bool keep = false;
        T val {};
        if (gid < n) {
            keep = (mask[gid] != 0);
            if (keep) {
                val = src[gid];
            }
        }

#if __CUDACC_VER_MAJOR__ >= 9
        unsigned int ballot = __ballot_sync(0xffffffffu, keep);
#else
        unsigned int ballot = __ballot(keep);
#endif
        const int warp_keep_prefix = warp_exclusive_prefix(ballot, lane);
        const int warp_keep_count = __popc(ballot);

        if (lane == 0) {
            warp_counts[warpIdInBlock] = warp_keep_count;
        }
        __syncthreads();

        if (warpIdInBlock == 0) {
            unsigned int acc = 0;
            for (unsigned int w = 0; w < numWarps; ++w) {
                warp_prefix[w] = acc; // exclusive
                acc += warp_counts[w];
            }
            if (lane == 0) {
                block_total = acc;
                (void)block_total; // Suppress clangd error: used outside of kernel
            }
        }
        __syncthreads();

        const unsigned int my_warp_base = warp_prefix[warpIdInBlock];
        const unsigned int my_local_pos = my_warp_base + warp_keep_prefix;
        const unsigned int block_global_base = block_offsets[blockIdx.x];

        if (keep) {
            dst[block_global_base + my_local_pos] = val;
        }
    }

} // namespace gpu

template <typename T>
size_t filter_compact(T* dst, const T* src, const uint8_t* mask, size_t n, cudaStream_t stream, int block)
{
    if (n == 0)
        return 0;

    const int grid = static_cast<int>((n + block - 1) / block);

    unsigned int* d_block_counts = nullptr;
    unsigned int* d_block_offsets = nullptr;
    CHECKED_CALL_THROW(cudaMalloc(&d_block_counts, grid * sizeof(unsigned int)));
    CHECKED_CALL_THROW(cudaMalloc(&d_block_offsets, grid * sizeof(unsigned int)));

    // 1. Count the masks in each block
    const size_t shmem_count = block * sizeof(unsigned int);
    gpu::countMaskPerBlock<<<grid, block, shmem_count, stream>>>(mask, n, d_block_counts);
    CHECKED_CALL_THROW(cudaGetLastError());

    // 2. Exclusive scan at host
    // TODO: Make it happen on device?
    unsigned int* h_counts;
    unsigned int* h_offsets;
    CHECKED_CALL_THROW(cudaMallocHost(&h_counts, grid * sizeof(unsigned int)));
    CHECKED_CALL_THROW(cudaMallocHost(&h_offsets, grid * sizeof(unsigned int)));
    CHECKED_CALL_THROW(
        cudaMemcpyAsync(h_counts, d_block_counts, grid * sizeof(unsigned int), cudaMemcpyDeviceToHost, stream));
    CHECKED_CALL_THROW(cudaStreamSynchronize(stream));
    h_offsets[0] = 0u;
    for (int i = 1; i < grid; ++i) {
        h_offsets[i] = h_offsets[i - 1] + h_counts[i - 1];
    }
    const unsigned int total_kept = h_offsets[grid - 1] + (grid > 0 ? h_counts[grid - 1] : 0u);
    CHECKED_CALL_THROW(
        cudaMemcpyAsync(d_block_offsets, h_offsets, grid * sizeof(unsigned int), cudaMemcpyHostToDevice, stream));
    CHECKED_CALL_THROW(cudaFreeHost(h_counts));
    CHECKED_CALL_THROW(cudaFreeHost(h_offsets));

    // 3. Stable compression
    const int numWarps = (block + 31) / 32;
    const size_t shmem_compact = 2 * numWarps * sizeof(unsigned int);
    gpu::filterCompactionKernel<T><<<grid, block, shmem_compact, stream>>>(dst, src, mask, n, d_block_offsets);
    CHECKED_CALL_THROW(cudaGetLastError());

    CHECKED_CALL_THROW(cudaFree(d_block_counts));
    CHECKED_CALL_THROW(cudaFree(d_block_offsets));

    return static_cast<size_t>(total_kept);
}

#define X(name, DT, VT) template size_t filter_compact<DT>(DT*, const DT*, const uint8_t*, size_t, cudaStream_t, int);
LIST_TYPES(X)
#undef X

} // namespace velodb
