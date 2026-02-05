#pragma once

#include "../common/types.hpp"

#include "../common/hash_table.cuh"
#include "../common/launcher.cuh"

#include <cooperative_groups.h>

namespace cg = cooperative_groups;

namespace gpu_native {

// ============================================================================
// Q06: Multi-predicate Filter (Single Table Scan with Revenue Calculation)
// ============================================================================
// SELECT l_extendedprice, l_discount
// FROM lineitem
// WHERE l_shipdate >= DATE '1994-01-01'
//   AND l_shipdate < DATE '1995-01-01'
//   AND l_discount BETWEEN 0.05 AND 0.07
//   AND l_quantity < 24.0;
// ============================================================================

struct Q06Result {
    double* extendedprice;
    double* discount;
    uint32_t* count;
    size_t capacity;
};

__global__ void q06_kernel(
    // Input: Lineitem columns
    const double* __restrict__ l_extendedprice,
    const double* __restrict__ l_discount,
    const double* __restrict__ l_quantity,
    const int32_t* __restrict__ l_shipdate,
    size_t n_lineitem,
    // Output
    double* __restrict__ out_extendedprice,
    double* __restrict__ out_discount,
    uint32_t* __restrict__ result_count)
{
    cg::grid_group grid = cg::this_grid();

    // Date constants
    constexpr int32_t DATE_START = 19940101; // 1994-01-01
    constexpr int32_t DATE_END = 19950101; // 1995-01-01

    // Grid-stride loop
    for (size_t i = grid.thread_rank(); i < n_lineitem; i += grid.size()) {
        int32_t shipdate = l_shipdate[i];
        double discount = l_discount[i];
        double quantity = l_quantity[i];

        bool pred = (shipdate >= DATE_START) && (shipdate < DATE_END) && (discount >= 0.05) && (discount <= 0.07)
            && (quantity < 24.0);

        if (pred) {
            uint32_t pos = atomicAdd(result_count, 1);
            out_extendedprice[pos] = l_extendedprice[i];
            out_discount[pos] = l_discount[i];
        }
    }
}

inline Q06Result allocate_q06_result(size_t capacity, cudaStream_t stream = 0)
{
    Q06Result result;
    result.capacity = capacity;

    GPU_CHECK(cudaMalloc(&result.extendedprice, capacity * sizeof(double)));
    GPU_CHECK(cudaMalloc(&result.discount, capacity * sizeof(double)));
    GPU_CHECK(cudaMalloc(&result.count, sizeof(uint32_t)));
    GPU_CHECK(cudaMemsetAsync(result.count, 0, sizeof(uint32_t), stream));

    return result;
}

inline void free_q06_result(Q06Result& result)
{
    cudaFree(result.extendedprice);
    cudaFree(result.discount);
    cudaFree(result.count);
}

inline uint32_t run_q06(const LineitemColumns& lineitem, Q06Result& result, cudaStream_t stream = 0)
{
    GPU_CHECK(cudaMemsetAsync(result.count, 0, sizeof(uint32_t), stream));

    launchQuery(q06_kernel,
                stream,
                lineitem.l_extendedprice,
                lineitem.l_discount,
                lineitem.l_quantity,
                lineitem.l_shipdate,
                lineitem.num_rows,
                result.extendedprice,
                result.discount,
                result.count);

    uint32_t count;
    GPU_CHECK(cudaMemcpyAsync(&count, result.count, sizeof(uint32_t), cudaMemcpyDeviceToHost, stream));
    GPU_CHECK(cudaStreamSynchronize(stream));

    return count;
}

} // namespace gpu_native
