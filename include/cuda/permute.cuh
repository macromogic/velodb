#pragma once

#include "cuda/commands.hpp"
#include "cuda/helper.hpp"

#include <cuda_runtime.h>

namespace velodb::cuda {

template <typename T>
__device__ __forceinline__ void executePermuteImpl(T* __restrict__ output,
                                                   const T* __restrict__ input,
                                                   const int32_t* __restrict__ scatter_indices,
                                                   const size_t n)
{
    const int tid = threadIdx.x;
    // Grid-Stride Loop
    for (uint32_t idx = blockIdx.x * blockDim.x + tid; idx < n; idx += gridDim.x * blockDim.x) {
        int32_t read_pos = scatter_indices[idx];
        output[idx] = input[read_pos];
    }
}

__device__ __forceinline__ void executePermute(CommandArgs::PermuteArgs& args, [[maybe_unused]] cg::grid_group& grid)
{
    switch (args.type_id) {
#define X(name, DT, VT)                                                                                                \
    case DataTypeId::name: {                                                                                           \
        executePermuteImpl<DT>(static_cast<DT*>(args.out_data),                                                        \
                               static_cast<const DT*>(args.in_data),                                                   \
                               args.in_indices,                                                                        \
                               args.n);                                                                                \
        break;                                                                                                         \
    }
        LIST_TYPES(X)
#undef X
    default:
        break;
    }
}

} // namespace velodb::cuda
