#include "common/profiler.hpp"
#include "cuda/compaction.hpp"
#include "cuda/helper.hpp"
#include "data/type_traits.hpp"

#include <cstdint>

#include <cuda_runtime.h>

namespace velodb {

namespace gpu {

    __device__ __forceinline__ int warpExclusivePrefix(unsigned int ballot, unsigned int lane)
    {
        unsigned int lower = ballot & ((1u << lane) - 1u);
        return __popc(lower);
    }

    __global__ void countMaskPerBlockKernel(const uint8_t* __restrict__ mask,
                                            size_t n,
                                            unsigned int* __restrict__ block_counts)
    {
        // Optimized reduction for H100: warp-level shuffle reductions with minimal shared memory
        extern __shared__ unsigned int warp_sums[]; // [num_warps]

        const unsigned int gid = blockIdx.x * blockDim.x + threadIdx.x;
        const unsigned int stride = gridDim.x * blockDim.x;
        const unsigned int lane = threadIdx.x & WARP_MASK;
        const unsigned int warp_id = threadIdx.x / WARP_SIZE;
        const unsigned int active = __activemask();

        unsigned int local = 0u;
        for (size_t i = gid; i < n; i += stride) {
            local += (mask[i] != 0);
        }

        // In-warp reduction
        unsigned int sum = local;
#pragma unroll
        for (int offset = WARP_SIZE >> 1; offset > 0; offset >>= 1) {
            sum += __shfl_down_sync(active, sum, offset);
        }
        if (lane == 0) {
            warp_sums[warp_id] = sum;
        }
        __syncthreads();

        // First warp reduces warp_sums to a single block sum
        if (warp_id == 0) {
            unsigned int warp_sum = (lane < DIV_UP(blockDim.x, WARP_SIZE)) ? warp_sums[lane] : 0u;
#pragma unroll
            for (int offset = WARP_SIZE >> 1; offset > 0; offset >>= 1) {
                warp_sum += __shfl_down_sync(0xffffffffu, warp_sum, offset);
            }
            if (lane == 0) {
                block_counts[blockIdx.x] = warp_sum;
            }
        }
    }

    template <typename T>
    __global__ void filterCompaction(T* __restrict__ dst,
                                     const T* __restrict__ src,
                                     const uint8_t* __restrict__ mask,
                                     size_t n,
                                     const unsigned int* __restrict__ block_offsets)
    {
        const unsigned int gid = blockIdx.x * blockDim.x + threadIdx.x;
        const unsigned int lane = threadIdx.x & WARP_MASK;
        const unsigned int warp_id = threadIdx.x / WARP_SIZE;
        const unsigned int num_warps = DIV_UP(blockDim.x, WARP_SIZE);

        extern __shared__ unsigned int shmem[];
        unsigned int* warp_counts = shmem; // [num_warps]
        unsigned int* warp_prefix = shmem + num_warps; // [num_warps]

        bool keep = false;
        T val {};
        if (gid < n) {
            keep = (mask[gid] != 0);
            if (keep) {
                val = src[gid];
            }
        }

        const unsigned int active = __activemask();
        unsigned int ballot = __ballot_sync(active, keep);
        const int warp_keep_prefix = warpExclusivePrefix(ballot, lane);
        const int warp_keep_count = __popc(ballot);

        if (lane == 0) {
            warp_counts[warp_id] = warp_keep_count;
        }
        __syncthreads();

        if (warp_id == 0) {
            unsigned int acc = 0;
            for (unsigned int w = 0; w < num_warps; ++w) {
                warp_prefix[w] = acc; // exclusive
                acc += warp_counts[w];
            }
        }
        __syncthreads();

        const unsigned int warp_base = warp_prefix[warp_id];
        const unsigned int local_pos = warp_base + warp_keep_prefix;
        const unsigned int block_global_base = block_offsets[blockIdx.x];

        if (keep) {
            dst[block_global_base + local_pos] = val;
        }
    }

    __global__ void filterCompactionBitmapKernel(BitVector::Element* __restrict__ dst,
                                                 const BitVector::Element* __restrict__ src,
                                                 const uint8_t* __restrict__ mask,
                                                 size_t n,
                                                 size_t bits_offset,
                                                 const unsigned int* __restrict__ block_offsets)
    {
        const unsigned int gid = blockIdx.x * blockDim.x + threadIdx.x;
        const unsigned int lane = threadIdx.x & WARP_MASK;
        const unsigned int warp_id = threadIdx.x / WARP_SIZE;
        const unsigned int num_warps = DIV_UP(blockDim.x, WARP_SIZE);

        extern __shared__ unsigned int shmem[];
        unsigned int* warp_counts = shmem; // [num_warps]
        unsigned int* warp_prefix = shmem + num_warps; // [num_warps]

        // Evaluate predicate and fetch source bit
        bool keep = false;
        unsigned int src_bit_val = 0u;
        if (gid < n) {
            keep = (mask[gid] != 0);
            if (keep) {
                const size_t word_idx = static_cast<size_t>(gid) / BitVector::ELEMENT_WIDTH;
                const unsigned int bit_idx = static_cast<unsigned int>(static_cast<size_t>(gid)
                                                                       % BitVector::ELEMENT_WIDTH);
                const BitVector::Element word = src[word_idx];
                src_bit_val = static_cast<unsigned int>((word >> bit_idx) & 1ull);
            }
        }

        const unsigned int active = __activemask();
        unsigned int ballot = __ballot_sync(active, keep);
        const int warp_keep_prefix = warpExclusivePrefix(ballot, lane);
        const int warp_keep_count = __popc(ballot);

        if (lane == 0) {
            warp_counts[warp_id] = static_cast<unsigned int>(warp_keep_count);
        }
        __syncthreads();

        if (warp_id == 0) {
            unsigned int acc = 0u;
            for (unsigned int w = 0; w < num_warps; ++w) {
                warp_prefix[w] = acc; // exclusive prefix of kept counts per warp
                acc += warp_counts[w];
            }
        }
        __syncthreads();

        const unsigned int warp_base = warp_prefix[warp_id];
        const unsigned int local_pos = warp_base + static_cast<unsigned int>(warp_keep_prefix);
        const unsigned int block_global_base = block_offsets[blockIdx.x];

        // Warp-grouped OR aggregation into dst words, avoiding per-thread atomics
        // and eliminating atomicAnd by requiring zero-initialized output range.
        const bool contributes = keep && (src_bit_val != 0u);
        size_t out_word_idx_sz = 0;
        unsigned int out_bit = 0;
        unsigned long long contrib_bits = 0ull;
        if (contributes) {
            const size_t out_bit_index = static_cast<size_t>(bits_offset) + static_cast<size_t>(block_global_base)
                + static_cast<size_t>(local_pos);
            out_word_idx_sz = out_bit_index / BitVector::ELEMENT_WIDTH;
            out_bit = static_cast<unsigned int>(out_bit_index % BitVector::ELEMENT_WIDTH);
            contrib_bits = (1ull << out_bit);
        }

        const unsigned int full_mask = __activemask();
        const unsigned int contrib_mask = __ballot_sync(full_mask, contributes);

        if (contributes) {
            const unsigned long long word_key = static_cast<unsigned long long>(out_word_idx_sz);
            const unsigned int group = __match_any_sync(contrib_mask, word_key);
            const int leader = __ffs(group) - 1; // lane index of leader within warp

            // Aggregate contributions within group via shuffles
            unsigned long long agg = contrib_bits;
            unsigned int remaining = group & ~(1u << lane);
            while (remaining) {
                const int peer = __ffs(remaining) - 1;
                const unsigned long long peer_bits = __shfl_sync(group, contrib_bits, peer);
                agg |= peer_bits;
                remaining &= (remaining - 1);
            }

            if (static_cast<int>(lane) == leader) {
                unsigned long long* addr = reinterpret_cast<unsigned long long*>(&dst[out_word_idx_sz]);
                atomicOr(addr, agg);
            }
        }
    }

    __global__ void scanTileExclusiveKernel(const unsigned int* __restrict__ in,
                                            unsigned int* __restrict__ out,
                                            unsigned int* __restrict__ block_sums,
                                            unsigned int n)
    {
        const unsigned int tid = threadIdx.x;
        const unsigned int BLOCK = blockDim.x;
        const unsigned int TILE = 2u * BLOCK;
        const unsigned int base = blockIdx.x * TILE;

        extern __shared__ unsigned int s[]; // size TILE
        const unsigned int ai = tid;
        const unsigned int bi = tid + BLOCK;

        s[ai] = (base + ai < n) ? in[base + ai] : 0u;
        s[bi] = (base + bi < n) ? in[base + bi] : 0u;
        __syncthreads();

        // Upsweep
        for (unsigned int offset = 1; offset < TILE; offset <<= 1) {
            unsigned int idx = (tid + 1) * (offset << 1) - 1;
            if (idx < TILE) {
                s[idx] += s[idx - offset];
            }
            __syncthreads();
        }

        if (tid == 0) {
            block_sums[blockIdx.x] = s[TILE - 1];
            s[TILE - 1] = 0u; // convert to exclusive
        }
        __syncthreads();

        // Downsweep
        for (unsigned int offset = TILE >> 1; offset > 0; offset >>= 1) {
            unsigned int idx = (tid + 1) * (offset << 1) - 1;
            if (idx < TILE) {
                unsigned int t = s[idx - offset];
                s[idx - offset] = s[idx];
                s[idx] += t;
            }
            __syncthreads();
        }

        if (base + ai < n) {
            out[base + ai] = s[ai];
        }
        if (base + bi < n) {
            out[base + bi] = s[bi];
        }
    }

    // Single-block exclusive scan for block_sums array (length m), padded to next power of two
    __global__ void scanSingleBlockExclusiveKernel(const unsigned int* __restrict__ in,
                                                   unsigned int* __restrict__ out,
                                                   unsigned int m,
                                                   unsigned int padded)
    {
        const unsigned int tid = threadIdx.x;
        const unsigned int BLOCK = blockDim.x; // expect 2*BLOCK == padded
        const unsigned int TILE = 2u * BLOCK;
        extern __shared__ unsigned int s[]; // size padded

        const unsigned int ai = tid;
        const unsigned int bi = tid + BLOCK;
        s[ai] = (ai < m) ? in[ai] : 0u;
        s[bi] = (bi < m) ? in[bi] : 0u;
        __syncthreads();

        for (unsigned int offset = 1; offset < TILE; offset <<= 1) {
            unsigned int idx = (tid + 1) * (offset << 1) - 1;
            if (idx < TILE) {
                s[idx] += s[idx - offset];
            }
            __syncthreads();
        }
        if (tid == 0) {
            s[TILE - 1] = 0u;
        }
        __syncthreads();
        for (unsigned int offset = TILE >> 1; offset > 0; offset >>= 1) {
            unsigned int idx = (tid + 1) * (offset << 1) - 1;
            if (idx < TILE) {
                unsigned int t = s[idx - offset];
                s[idx - offset] = s[idx];
                s[idx] += t;
            }
            __syncthreads();
        }
        if (ai < m)
            out[ai] = s[ai];
        if (bi < m)
            out[bi] = s[bi];
    }

    // Uniform add scanned block offsets to each tile of the per-element scan
    __global__ void uniformAddKernel(unsigned int* __restrict__ data,
                                     unsigned int n,
                                     const unsigned int* __restrict__ block_offsets,
                                     unsigned int tile)
    {
        const unsigned int tid = threadIdx.x;
        const unsigned int base = blockIdx.x * tile;
        const unsigned int off = block_offsets[blockIdx.x];

        unsigned int i0 = base + tid;
        unsigned int i1 = base + tid + blockDim.x;
        if (i0 < n)
            data[i0] += off;
        if (i1 < n)
            data[i1] += off;
    }

} // namespace gpu

namespace {

    inline unsigned int nextPow2(unsigned int x)
    {
        if (x <= 1u)
            return 1u;
        x--;
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        x++;
        return x;
    }

} // anonymous namespace

template <typename T>
size_t filterCompact(T* dst_data,
                     BitVector::Element* dst_bitmap,
                     const T* src_data,
                     const BitVector::Element* src_bitmap,
                     const uint8_t* mask,
                     size_t n,
                     size_t bits_offset,
                     cudaStream_t stream,
                     unsigned int block)
{
    PROFILE_FUNCTION();
    if (n == 0)
        return 0;

    const unsigned int grid = static_cast<unsigned int>(DIV_UP(n, block));

    unsigned int* d_block_counts = nullptr;
    unsigned int* d_block_offsets = nullptr;
    CHECKED_CALL_THROW(cudaMalloc(&d_block_counts, grid * sizeof(unsigned int)));
    CHECKED_CALL_THROW(cudaMalloc(&d_block_offsets, grid * sizeof(unsigned int)));

    // 1. Count the masks in each block
    const unsigned int num_warps = DIV_UP(block, WARP_SIZE);
    const size_t shmem_count = num_warps * sizeof(unsigned int);
    gpu::countMaskPerBlockKernel<<<grid, block, shmem_count, stream>>>(mask, n, d_block_counts);
    CHECKED_CALL_THROW(cudaGetLastError());

    // 2. Exclusive scan on device (no CUB): two-stage scan + uniform add
    const unsigned int SCAN_BLOCK = 256u; // tuned default for H100
    const unsigned int TILE = 2u * SCAN_BLOCK;
    const unsigned int num_tiles = (grid + TILE - 1u) / TILE;

    unsigned int* d_block_sums = nullptr;
    unsigned int* d_block_sums_offsets = nullptr;
    CHECKED_CALL_THROW(cudaMalloc(&d_block_sums, (num_tiles ? num_tiles : 1u) * sizeof(unsigned int)));
    if (num_tiles > 1u) {
        CHECKED_CALL_THROW(cudaMalloc(&d_block_sums_offsets, num_tiles * sizeof(unsigned int)));
    }

    // Stage 1: per-tile scan
    gpu::scanTileExclusiveKernel<<<num_tiles, SCAN_BLOCK, TILE * sizeof(unsigned int), stream>>>(d_block_counts,
                                                                                                 d_block_offsets,
                                                                                                 d_block_sums,
                                                                                                 grid);
    CHECKED_CALL_THROW(cudaGetLastError());

    // Stage 2: scan tile sums if multiple tiles
    if (num_tiles > 1u) {
        const unsigned int padded = nextPow2(num_tiles);
        const unsigned int BLOCK2 = padded / 2u; // since TILE2 = 2*BLOCK2
        gpu::scanSingleBlockExclusiveKernel<<<1, BLOCK2, padded * sizeof(unsigned int), stream>>>(d_block_sums,
                                                                                                  d_block_sums_offsets,
                                                                                                  num_tiles,
                                                                                                  padded);
        CHECKED_CALL_THROW(cudaGetLastError());

        // Stage 3: uniform add
        gpu::uniformAddKernel<<<num_tiles, SCAN_BLOCK, 0, stream>>>(d_block_offsets, grid, d_block_sums_offsets, TILE);
        CHECKED_CALL_THROW(cudaGetLastError());
    }

    // 3. Stable compression
    const size_t shmem_compact = 2 * num_warps * sizeof(unsigned int);
    gpu::filterCompaction<T><<<grid, block, shmem_compact, stream>>>(dst_data, src_data, mask, n, d_block_offsets);
    CHECKED_CALL_THROW(cudaGetLastError());
    gpu::filterCompactionBitmapKernel<<<grid, block, shmem_compact, stream>>>(dst_bitmap,
                                                                              src_bitmap,
                                                                              mask,
                                                                              n,
                                                                              bits_offset,
                                                                              d_block_offsets);

    // 4. Retrieve total kept elements (last offset + last count)
    unsigned int* h_last_off = nullptr;
    unsigned int* h_last_cnt = nullptr;
    CHECKED_CALL_THROW(cudaMallocHost(&h_last_off, sizeof(unsigned int)));
    CHECKED_CALL_THROW(cudaMallocHost(&h_last_cnt, sizeof(unsigned int)));
    if (grid > 0) {
        CHECKED_CALL_THROW(cudaMemcpyAsync(h_last_off,
                                           d_block_offsets + (grid - 1),
                                           sizeof(unsigned int),
                                           cudaMemcpyDeviceToHost,
                                           stream));
        CHECKED_CALL_THROW(cudaMemcpyAsync(h_last_cnt,
                                           d_block_counts + (grid - 1),
                                           sizeof(unsigned int),
                                           cudaMemcpyDeviceToHost,
                                           stream));
    }

    // Ensure all work on the stream completes before computing/returning
    CHECKED_CALL_THROW(cudaStreamSynchronize(stream));

    const size_t total_kept = (grid > 0) ? (static_cast<size_t>(*h_last_off) + static_cast<size_t>(*h_last_cnt)) : 0u;

    if (h_last_off) {
        CHECKED_CALL_THROW(cudaFreeHost(h_last_off));
    }
    if (h_last_cnt) {
        CHECKED_CALL_THROW(cudaFreeHost(h_last_cnt));
    }

    if (d_block_sums_offsets) {
        CHECKED_CALL_THROW(cudaFree(d_block_sums_offsets));
    }
    if (d_block_sums) {
        CHECKED_CALL_THROW(cudaFree(d_block_sums));
    }
    CHECKED_CALL_THROW(cudaFree(d_block_counts));
    CHECKED_CALL_THROW(cudaFree(d_block_offsets));

    return total_kept;
}

#define X(name, DT, VT)                                                                                                \
    template size_t filterCompact<DT>(DT*,                                                                             \
                                      BitVector::Element*,                                                             \
                                      const DT*,                                                                       \
                                      const BitVector::Element*,                                                       \
                                      const uint8_t*,                                                                  \
                                      size_t,                                                                          \
                                      size_t,                                                                          \
                                      cudaStream_t,                                                                    \
                                      unsigned int);
LIST_TYPES(X)
#undef X

} // namespace velodb
