#pragma once

#include "../common/types.hpp"

#include "../common/hash_table.cuh"
#include "../common/launcher.cuh"

#include <cooperative_groups.h>

namespace cg = cooperative_groups;

namespace gpu_native {

// ============================================================================
// Q01: Simple Filter Query (Single Table Scan)
// ============================================================================
// SELECT l_returnflag, l_linestatus, l_quantity, l_extendedprice, l_discount, l_tax
// FROM lineitem
// WHERE l_shipdate <= DATE '1998-09-02';
// ============================================================================

struct Q01Result {
    int8_t* returnflag;
    int8_t* linestatus;
    double* quantity;
    double* extendedprice;
    double* discount;
    double* tax;
    uint32_t* count;
    size_t capacity;
};

__global__ void q01_kernel(
    // Input: Lineitem columns
    const int8_t* __restrict__ l_returnflag,
    const int8_t* __restrict__ l_linestatus,
    const double* __restrict__ l_quantity,
    const double* __restrict__ l_extendedprice,
    const double* __restrict__ l_discount,
    const double* __restrict__ l_tax,
    const int32_t* __restrict__ l_shipdate,
    size_t n_lineitem,
    // Output
    int8_t* __restrict__ out_returnflag,
    int8_t* __restrict__ out_linestatus,
    double* __restrict__ out_quantity,
    double* __restrict__ out_extendedprice,
    double* __restrict__ out_discount,
    double* __restrict__ out_tax,
    uint32_t* __restrict__ result_count)
{
    cg::grid_group grid = cg::this_grid();

    // Date constant: 1998-09-02 = 19980902
    constexpr int32_t DATE_THRESHOLD = 19980902;

    // Grid-stride loop over lineitem
    for (size_t i = grid.thread_rank(); i < n_lineitem; i += grid.size()) {
        if (l_shipdate[i] <= DATE_THRESHOLD) {
            // Atomic append to output
            uint32_t pos = atomicAdd(result_count, 1);

            out_returnflag[pos] = l_returnflag[i];
            out_linestatus[pos] = l_linestatus[i];
            out_quantity[pos] = l_quantity[i];
            out_extendedprice[pos] = l_extendedprice[i];
            out_discount[pos] = l_discount[i];
            out_tax[pos] = l_tax[i];
        }
    }
}

inline Q01Result allocate_q01_result(size_t capacity, cudaStream_t stream = 0)
{
    Q01Result result;
    result.capacity = capacity;

    GPU_CHECK(cudaMalloc(&result.returnflag, capacity * sizeof(int8_t)));
    GPU_CHECK(cudaMalloc(&result.linestatus, capacity * sizeof(int8_t)));
    GPU_CHECK(cudaMalloc(&result.quantity, capacity * sizeof(double)));
    GPU_CHECK(cudaMalloc(&result.extendedprice, capacity * sizeof(double)));
    GPU_CHECK(cudaMalloc(&result.discount, capacity * sizeof(double)));
    GPU_CHECK(cudaMalloc(&result.tax, capacity * sizeof(double)));
    GPU_CHECK(cudaMalloc(&result.count, sizeof(uint32_t)));
    GPU_CHECK(cudaMemsetAsync(result.count, 0, sizeof(uint32_t), stream));

    return result;
}

inline void free_q01_result(Q01Result& result)
{
    cudaFree(result.returnflag);
    cudaFree(result.linestatus);
    cudaFree(result.quantity);
    cudaFree(result.extendedprice);
    cudaFree(result.discount);
    cudaFree(result.tax);
    cudaFree(result.count);
}

inline uint32_t run_q01(const LineitemColumns& lineitem, Q01Result& result, cudaStream_t stream = 0)
{
    // Reset result count
    GPU_CHECK(cudaMemsetAsync(result.count, 0, sizeof(uint32_t), stream));

    launchQuery(q01_kernel,
                stream,
                lineitem.l_returnflag,
                lineitem.l_linestatus,
                lineitem.l_quantity,
                lineitem.l_extendedprice,
                lineitem.l_discount,
                lineitem.l_tax,
                lineitem.l_shipdate,
                lineitem.num_rows,
                result.returnflag,
                result.linestatus,
                result.quantity,
                result.extendedprice,
                result.discount,
                result.tax,
                result.count);

    uint32_t count;
    GPU_CHECK(cudaMemcpyAsync(&count, result.count, sizeof(uint32_t), cudaMemcpyDeviceToHost, stream));
    GPU_CHECK(cudaStreamSynchronize(stream));

    return count;
}

} // namespace gpu_native
