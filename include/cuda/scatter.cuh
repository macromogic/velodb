#pragma once

#include "cuda/commands.hpp"
#include "cuda/helper.hpp"

#include <cuda_runtime.h>

namespace velodb::cuda {

__device__ __forceinline__ uint32_t warpExclusivePrefix(uint32_t ballot, int lane)
{
    uint32_t lower = ballot & ((1u << lane) - 1u);
    return __popc(lower);
}

__device__ __forceinline__ void executeScatter(CommandArgs::ScatterArgs& args, [[maybe_unused]] cg::grid_group& grid)
{
    constexpr int BLOCK_THREADS = 256;
    constexpr int NUM_WARPS = BLOCK_THREADS / 32;
    __shared__ uint32_t smem_warp_counts[NUM_WARPS];
    __shared__ uint32_t smem_warp_offsets[NUM_WARPS];
    __shared__ size_t smem_block_base;

    const uint8_t* __restrict__ mask = args.in_mask;
    int32_t* __restrict__ scatter_indices = args.out_indices;
    size_t* __restrict__ global_count_ptr = args.out_count;
    const size_t n = args.n;

    const int tid = threadIdx.x;
    const int lane = tid % 32;
    const int warp_id = tid / 32;
    uint32_t stride = blockDim.x * gridDim.x;
    for (uint32_t base_idx = blockIdx.x * blockDim.x; base_idx < n; base_idx += stride) {
        uint32_t idx = base_idx + tid;
        uint8_t m = 0;
        if (idx < n) {
            m = mask[idx];
        }
        bool keep = (m != 0);

        // Warp-level prefixes
        uint32_t active_mask = __activemask();
        uint32_t ballot = __ballot_sync(active_mask, keep);
        uint32_t my_warp_cnt = __popc(ballot);
        uint32_t my_warp_prefix = warpExclusivePrefix(ballot, lane);

        // Block-level scan to get warp offsets
        if (lane == 0) {
            smem_warp_counts[warp_id] = my_warp_cnt;
        }
        __syncthreads();

        if (warp_id == 0) {
            size_t scan_val = (lane < NUM_WARPS) ? smem_warp_counts[lane] : 0;
#pragma unroll
            // Simple Warp Scan
            for (int i = 1; i < NUM_WARPS; i *= 2) {
                size_t temp = __shfl_up_sync(0xffffffff, scan_val, i);
                if (lane >= i)
                    scan_val += temp;
            }
            if (lane < 8) {
                smem_warp_offsets[lane] = scan_val - smem_warp_counts[lane];
            }
            // The last lane writes the total count
            if (lane == 7) {
                if (scan_val > 0) {
                    smem_block_base = atomicAdd(reinterpret_cast<unsigned long long*>(global_count_ptr),
                                                static_cast<unsigned long long>(scan_val));
                }
            }
        }
        __syncthreads();

        if (keep) {
            uint32_t global_write_idx = smem_block_base + smem_warp_offsets[warp_id] + my_warp_prefix;
            scatter_indices[idx] = static_cast<int32_t>(global_write_idx);
        }
        __syncthreads();
    }
}

} // namespace velodb::cuda
