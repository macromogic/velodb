#pragma once

#include "cuda/commands.hpp"
#include "data/data_type.hpp"
#include "data/type_traits.hpp"

#include <cooperative_groups.h>

namespace velodb::cuda {

namespace cg = cooperative_groups;

template <typename T>
__device__ __forceinline__ void executeCompareEqTyped(const T* __restrict__ left_data,
                                                      const T* __restrict__ right_data,
                                                      uint8_t* __restrict__ out_mask,
                                                      size_t n,
                                                      cg::grid_group& grid)
{
    for (size_t i = grid.thread_rank(); i < n; i += grid.size()) {
        out_mask[i] = (left_data[i] == right_data[i]) ? 1 : 0;
    }
}

__device__ __forceinline__ void executeCompareEq(const CommandArgs::CompareEqArgs& args, cg::grid_group& grid)
{
    switch (args.type_id) {
    case DataTypeId::TINYINT:
        executeCompareEqTyped<int8_t>(static_cast<const int8_t*>(args.left_data),
                                      static_cast<const int8_t*>(args.right_data),
                                      args.out_mask,
                                      args.n,
                                      grid);
        break;

    case DataTypeId::SMALLINT:
        executeCompareEqTyped<int16_t>(static_cast<const int16_t*>(args.left_data),
                                       static_cast<const int16_t*>(args.right_data),
                                       args.out_mask,
                                       args.n,
                                       grid);
        break;

    case DataTypeId::INTEGER:
        executeCompareEqTyped<int32_t>(static_cast<const int32_t*>(args.left_data),
                                       static_cast<const int32_t*>(args.right_data),
                                       args.out_mask,
                                       args.n,
                                       grid);
        break;

    case DataTypeId::BIGINT:
        executeCompareEqTyped<int64_t>(static_cast<const int64_t*>(args.left_data),
                                       static_cast<const int64_t*>(args.right_data),
                                       args.out_mask,
                                       args.n,
                                       grid);
        break;

    case DataTypeId::FLOAT:
        executeCompareEqTyped<float>(static_cast<const float*>(args.left_data),
                                     static_cast<const float*>(args.right_data),
                                     args.out_mask,
                                     args.n,
                                     grid);
        break;

    case DataTypeId::DOUBLE:
        executeCompareEqTyped<double>(static_cast<const double*>(args.left_data),
                                      static_cast<const double*>(args.right_data),
                                      args.out_mask,
                                      args.n,
                                      grid);
        break;

    case DataTypeId::DATE:
        executeCompareEqTyped<int32_t>(static_cast<const int32_t*>(args.left_data),
                                       static_cast<const int32_t*>(args.right_data),
                                       args.out_mask,
                                       args.n,
                                       grid);
        break;

    default:
        // Unsupported type, mask all as false
        for (size_t i = grid.thread_rank(); i < args.n; i += grid.size()) {
            args.out_mask[i] = 0;
        }
        break;
    }
}

} // namespace velodb::cuda
