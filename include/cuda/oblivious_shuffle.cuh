#pragma once

#include "cuda/commands.hpp"
#include "cuda/helper.hpp"

#include <cuda_runtime.h>

namespace velodb::cuda {

/**
 * @brief Compose position map with permutation (device function)
 *
 * new_map[i] = old_map[sigma[i]]
 */
__device__ __forceinline__ void executeComposePositionMap(CommandArgs::ComposePositionMapArgs& args,
                                                          [[maybe_unused]] cg::grid_group& grid)
{
    const uint32_t* __restrict__ old_map = args.old_map;
    const uint32_t* __restrict__ sigma = args.sigma;
    uint32_t* __restrict__ new_map = args.new_map;
    const size_t n = args.n;

    for (size_t i = blockIdx.x * blockDim.x + threadIdx.x; i < n; i += gridDim.x * blockDim.x) {
        new_map[i] = old_map[sigma[i]];
    }
}

/**
 * @brief Enrich record IDs with physical positions (device function)
 *
 * positions[i] = position_map[record_ids[i]]
 */
__device__ __forceinline__ void executeEnrichPositions(CommandArgs::EnrichPositionsArgs& args,
                                                       [[maybe_unused]] cg::grid_group& grid)
{
    const uint32_t* __restrict__ record_ids = args.record_ids;
    const uint32_t* __restrict__ position_map = args.position_map;
    uint32_t* __restrict__ positions = args.positions;
    const size_t n = args.n;

    for (size_t i = blockIdx.x * blockDim.x + threadIdx.x; i < n; i += gridDim.x * blockDim.x) {
        positions[i] = position_map[record_ids[i]];
    }
}

} // namespace velodb::cuda
