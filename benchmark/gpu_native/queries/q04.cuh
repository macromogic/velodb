#pragma once

#include "../common/types.hpp"

#include "../common/hash_table.cuh"
#include "../common/launcher.cuh"
#include "../common/sort_utils.cuh"

#include <cooperative_groups.h>
#include <thrust/device_ptr.h>
#include <thrust/sort.h>

namespace cg = cooperative_groups;

namespace gpu_native {

// ============================================================================
// Q04: Orders-Lineitem Join with Date Filter
// ============================================================================
// SELECT o_orderpriority
// FROM orders JOIN lineitem ON o_orderkey = l_orderkey
// WHERE o_orderdate >= DATE '1993-07-01'
//   AND o_orderdate < DATE '1993-10-01'
//   AND l_commitdate < l_receiptdate
// ORDER BY o_orderpriority;
// ============================================================================

struct Q04Result {
    int32_t* orderpriority;
    uint32_t* count;
    size_t capacity;
};

__global__ void q04_kernel(
    // Orders table (build side)
    const int32_t* __restrict__ o_orderkey,
    const int32_t* __restrict__ o_orderdate,
    const int32_t* __restrict__ o_orderpriority,
    size_t n_orders,
    // Lineitem table (probe side with filter)
    const int32_t* __restrict__ l_orderkey,
    const int32_t* __restrict__ l_commitdate,
    const int32_t* __restrict__ l_receiptdate,
    size_t n_lineitem,
    // Hash table for orders (orderkey -> orderpriority)
    HashEntry* __restrict__ ht_entries,
    uint32_t* __restrict__ ht_heads,
    uint32_t* __restrict__ ht_counter,
    uint32_t ht_num_buckets,
    uint32_t ht_capacity,
    // Output
    int32_t* __restrict__ out_orderpriority,
    uint32_t* __restrict__ result_count)
{
    cg::grid_group grid = cg::this_grid();

    // Date constants
    constexpr int32_t DATE_START = 19930701; // 1993-07-01
    constexpr int32_t DATE_END = 19931001; // 1993-10-01

    // ========== PHASE 1: Build hash table of filtered orders ==========
    // Filter by date, insert orderkey -> orderpriority
    for (size_t i = grid.thread_rank(); i < n_orders; i += grid.size()) {
        int32_t orderdate = o_orderdate[i];
        if (orderdate >= DATE_START && orderdate < DATE_END) {
            hash_table_insert(ht_entries,
                              ht_heads,
                              ht_counter,
                              ht_num_buckets,
                              ht_capacity,
                              o_orderkey[i],
                              o_orderpriority[i]);
        }
    }

    grid.sync(); // ===== SYNC =====

    // ========== PHASE 2: Probe lineitem, filter, output ==========
    // For each qualifying lineitem, output matching order's priority
    for (size_t i = grid.thread_rank(); i < n_lineitem; i += grid.size()) {
        if (l_commitdate[i] < l_receiptdate[i]) {
            int32_t orderkey = l_orderkey[i];

            // Probe orders hash table
            int32_t orderpriority = hash_table_probe(ht_entries, ht_heads, ht_num_buckets, orderkey);

            if (orderpriority >= 0) {
                uint32_t pos = atomicAdd(result_count, 1);
                out_orderpriority[pos] = orderpriority;
            }
        }
    }
}

inline Q04Result allocate_q04_result(size_t capacity, cudaStream_t stream = 0)
{
    Q04Result result;
    result.capacity = capacity;

    GPU_CHECK(cudaMalloc(&result.orderpriority, capacity * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&result.count, sizeof(uint32_t)));
    GPU_CHECK(cudaMemsetAsync(result.count, 0, sizeof(uint32_t), stream));

    return result;
}

inline void free_q04_result(Q04Result& result)
{
    cudaFree(result.orderpriority);
    cudaFree(result.count);
}

inline uint32_t run_q04(const OrdersColumns& orders,
                        const LineitemColumns& lineitem,
                        Q04Result& result,
                        cudaStream_t stream = 0)
{
    // Allocate hash table for filtered orders
    // Build side: orders filtered by date, smaller than lineitem
    HashTable ht = allocate_hash_table(orders.num_rows, stream);

    // Reset result count
    GPU_CHECK(cudaMemsetAsync(result.count, 0, sizeof(uint32_t), stream));

    launchQuery(q04_kernel,
                stream,
                // Orders (build side)
                orders.o_orderkey,
                orders.o_orderdate,
                orders.o_orderpriority,
                orders.num_rows,
                // Lineitem (probe side)
                lineitem.l_orderkey,
                lineitem.l_commitdate,
                lineitem.l_receiptdate,
                lineitem.num_rows,
                // Hash table
                ht.entries,
                ht.heads,
                ht.counter,
                ht.num_buckets,
                ht.capacity,
                // Output
                result.orderpriority,
                result.count);

    uint32_t count;
    GPU_CHECK(cudaMemcpyAsync(&count, result.count, sizeof(uint32_t), cudaMemcpyDeviceToHost, stream));
    GPU_CHECK(cudaStreamSynchronize(stream));

    // Cleanup
    free_hash_table(ht);

    // ORDER BY o_orderpriority
    if (count > 0) {
        thrust::device_ptr<int32_t> d_priority(result.orderpriority);
        thrust::sort(thrust::device, d_priority, d_priority + count);
    }

    return count;
}

} // namespace gpu_native
