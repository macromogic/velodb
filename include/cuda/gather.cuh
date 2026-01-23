#pragma once

#include "cuda/commands.hpp"
#include "cuda/helper.hpp"

#include <cuda_runtime.h>

namespace velodb::cuda {

template <typename T>
__device__ __forceinline__ void executeGatherImpl(T* __restrict__ output,
                                                  const T* __restrict__ input,
                                                  const int32_t* __restrict__ gather_indices,
                                                  const uint8_t* __restrict__ mask,
                                                  const size_t n)
{
    const int tid = threadIdx.x;
    // Grid-Stride Loop
    for (uint32_t idx = blockIdx.x * blockDim.x + tid; idx < n; idx += gridDim.x * blockDim.x) {
        bool keep = (mask == nullptr) || (mask[idx] != 0);
        if (keep) {
            int32_t write_pos = gather_indices[idx];
            output[write_pos] = input[idx];
        }
    }
}

__device__ void executeGather(CommandArgs::GatherArgs& args, [[maybe_unused]] cg::grid_group& grid)
{
    switch (args.type_id) {
#define X(name, DT, VT)                                                                                                \
    case DataTypeId::name: {                                                                                           \
        executeGatherImpl<DT>(static_cast<DT*>(args.out_data),                                                         \
                              static_cast<const DT*>(args.in_data),                                                    \
                              args.in_indices,                                                                         \
                              args.in_mask,                                                                            \
                              args.n);                                                                                 \
        break;                                                                                                         \
    }
        LIST_TYPES(X)
#undef X
    default:
        break;
    }
}

} // namespace velodb::cuda
