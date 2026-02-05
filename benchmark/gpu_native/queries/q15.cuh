#pragma once

#include "../common/types.hpp"

#include "../common/hash_table.cuh"
#include "../common/launcher.cuh"
#include "../common/sort_utils.cuh"

#include <cooperative_groups.h>
#include <thrust/device_ptr.h>
#include <thrust/gather.h>
#include <thrust/sort.h>

namespace cg = cooperative_groups;

namespace gpu_native {

// ============================================================================
// Q15: Supplier-Lineitem Join (Revenue Query)
// ============================================================================
// SELECT s_suppkey, s_name, s_address, s_phone, l_extendedprice, l_discount
// FROM supplier JOIN lineitem ON s_suppkey = l_suppkey
// ORDER BY s_suppkey;
// ============================================================================

struct Q15Result {
    int32_t* suppkey;
    double* extendedprice;
    double* discount;
    uint32_t* count;
    size_t capacity;
};

__global__ void q15_kernel(
    // Supplier table (build side)
    const int32_t* __restrict__ s_suppkey,
    size_t n_supplier,
    // Lineitem table (probe side)
    const int32_t* __restrict__ l_suppkey,
    const double* __restrict__ l_extendedprice,
    const double* __restrict__ l_discount,
    size_t n_lineitem,
    // Hash table: Supplier (suppkey -> row)
    HashEntry* __restrict__ ht_entries,
    uint32_t* __restrict__ ht_heads,
    uint32_t* __restrict__ ht_counter,
    uint32_t ht_num_buckets,
    uint32_t ht_capacity,
    // Output
    int32_t* __restrict__ out_suppkey,
    double* __restrict__ out_extendedprice,
    double* __restrict__ out_discount,
    uint32_t* __restrict__ result_count)
{
    cg::grid_group grid = cg::this_grid();

    // ========== PHASE 1: Build hash table on supplier ==========
    for (size_t i = grid.thread_rank(); i < n_supplier; i += grid.size()) {
        hash_table_insert(ht_entries,
                          ht_heads,
                          ht_counter,
                          ht_num_buckets,
                          ht_capacity,
                          s_suppkey[i],
                          static_cast<int32_t>(i));
    }

    grid.sync(); // ===== SYNC =====

    // ========== PHASE 2: Probe lineitem ==========
    for (size_t i = grid.thread_rank(); i < n_lineitem; i += grid.size()) {
        int32_t suppkey = l_suppkey[i];

        // Check if supplier exists
        if (hash_table_exists(ht_entries, ht_heads, ht_num_buckets, suppkey)) {
            uint32_t pos = atomicAdd(result_count, 1);
            out_suppkey[pos] = suppkey;
            out_extendedprice[pos] = l_extendedprice[i];
            out_discount[pos] = l_discount[i];
        }
    }
}

inline Q15Result allocate_q15_result(size_t capacity, cudaStream_t stream = 0)
{
    Q15Result result;
    result.capacity = capacity;

    GPU_CHECK(cudaMalloc(&result.suppkey, capacity * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&result.extendedprice, capacity * sizeof(double)));
    GPU_CHECK(cudaMalloc(&result.discount, capacity * sizeof(double)));
    GPU_CHECK(cudaMalloc(&result.count, sizeof(uint32_t)));
    GPU_CHECK(cudaMemsetAsync(result.count, 0, sizeof(uint32_t), stream));

    return result;
}

inline void free_q15_result(Q15Result& result)
{
    cudaFree(result.suppkey);
    cudaFree(result.extendedprice);
    cudaFree(result.discount);
    cudaFree(result.count);
}

inline uint32_t run_q15(const SupplierColumns& supplier,
                        const LineitemColumns& lineitem,
                        Q15Result& result,
                        cudaStream_t stream = 0)
{
    HashTable ht = allocate_hash_table(supplier.num_rows, stream);

    GPU_CHECK(cudaMemsetAsync(result.count, 0, sizeof(uint32_t), stream));

    launchQuery(q15_kernel,
                stream,
                // Supplier
                supplier.s_suppkey,
                supplier.num_rows,
                // Lineitem
                lineitem.l_suppkey,
                lineitem.l_extendedprice,
                lineitem.l_discount,
                lineitem.num_rows,
                // Hash table
                ht.entries,
                ht.heads,
                ht.counter,
                ht.num_buckets,
                ht.capacity,
                // Output
                result.suppkey,
                result.extendedprice,
                result.discount,
                result.count);

    uint32_t count;
    GPU_CHECK(cudaMemcpyAsync(&count, result.count, sizeof(uint32_t), cudaMemcpyDeviceToHost, stream));
    GPU_CHECK(cudaStreamSynchronize(stream));

    free_hash_table(ht);

    // ORDER BY s_suppkey
    if (count > 0) {
        // Allocate indices and sort by suppkey
        int32_t* d_indices;
        GPU_CHECK(cudaMalloc(&d_indices, count * sizeof(int32_t)));
        sort_indices_by_key(result.suppkey, d_indices, count, stream);

        // Reorder all columns using indices
        reorder_by_indices(d_indices, count, stream, result.suppkey, result.extendedprice, result.discount);

        GPU_CHECK(cudaFree(d_indices));
    }

    return count;
}

} // namespace gpu_native
