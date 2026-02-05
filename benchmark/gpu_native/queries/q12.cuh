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
// Q12: Two-way Join with Shipmode Filter
// ============================================================================
// SELECT l_shipmode, o_orderpriority
// FROM orders JOIN lineitem ON o_orderkey = l_orderkey
// WHERE (l_shipmode = 'MAIL' OR l_shipmode = 'SHIP')
//   AND l_commitdate < l_receiptdate
//   AND l_shipdate < l_commitdate
//   AND l_receiptdate >= DATE '1994-01-01'
//   AND l_receiptdate < DATE '1995-01-01'
// ORDER BY l_shipmode;
// ============================================================================

struct Q12Result {
    int32_t* shipmode;
    int32_t* orderpriority;
    uint32_t* count;
    size_t capacity;
};

__global__ void q12_kernel(
    // Orders table (build side)
    const int32_t* __restrict__ o_orderkey,
    const int32_t* __restrict__ o_orderpriority,
    size_t n_orders,
    // Lineitem table (probe side with filter)
    const int32_t* __restrict__ l_orderkey,
    const int32_t* __restrict__ l_shipmode,
    const int32_t* __restrict__ l_commitdate,
    const int32_t* __restrict__ l_receiptdate,
    const int32_t* __restrict__ l_shipdate,
    size_t n_lineitem,
    // Hash table: Orders (orderkey -> orderpriority)
    HashEntry* __restrict__ ht_entries,
    uint32_t* __restrict__ ht_heads,
    uint32_t* __restrict__ ht_counter,
    uint32_t ht_num_buckets,
    uint32_t ht_capacity,
    // Output
    int32_t* __restrict__ out_shipmode,
    int32_t* __restrict__ out_orderpriority,
    uint32_t* __restrict__ result_count)
{
    cg::grid_group grid = cg::this_grid();

    constexpr int32_t DATE_START = 19940101; // 1994-01-01
    constexpr int32_t DATE_END = 19950101; // 1995-01-01
    constexpr int32_t SHIPMODE_MAIL = 5; // From encoding
    constexpr int32_t SHIPMODE_SHIP = 3;

    // ========== PHASE 1: Build hash table on orders ==========
    for (size_t i = grid.thread_rank(); i < n_orders; i += grid.size()) {
        hash_table_insert(ht_entries,
                          ht_heads,
                          ht_counter,
                          ht_num_buckets,
                          ht_capacity,
                          o_orderkey[i],
                          o_orderpriority[i]);
    }

    grid.sync(); // ===== SYNC =====

    // ========== PHASE 2: Filter lineitem and probe orders ==========
    for (size_t i = grid.thread_rank(); i < n_lineitem; i += grid.size()) {
        int32_t shipmode = l_shipmode[i];
        int32_t commitdate = l_commitdate[i];
        int32_t receiptdate = l_receiptdate[i];
        int32_t shipdate = l_shipdate[i];

        // Check all predicates
        bool shipmode_ok = (shipmode == SHIPMODE_MAIL) || (shipmode == SHIPMODE_SHIP);
        bool date_ok = (commitdate < receiptdate) && (shipdate < commitdate) && (receiptdate >= DATE_START)
            && (receiptdate < DATE_END);

        if (shipmode_ok && date_ok) {
            int32_t orderkey = l_orderkey[i];

            // Probe orders hash table
            int32_t orderpriority = hash_table_probe(ht_entries, ht_heads, ht_num_buckets, orderkey);

            if (orderpriority >= 0) {
                uint32_t pos = atomicAdd(result_count, 1);
                out_shipmode[pos] = shipmode;
                out_orderpriority[pos] = orderpriority;
            }
        }
    }
}

inline Q12Result allocate_q12_result(size_t capacity, cudaStream_t stream = 0)
{
    Q12Result result;
    result.capacity = capacity;

    GPU_CHECK(cudaMalloc(&result.shipmode, capacity * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&result.orderpriority, capacity * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&result.count, sizeof(uint32_t)));
    GPU_CHECK(cudaMemsetAsync(result.count, 0, sizeof(uint32_t), stream));

    return result;
}

inline void free_q12_result(Q12Result& result)
{
    cudaFree(result.shipmode);
    cudaFree(result.orderpriority);
    cudaFree(result.count);
}

inline uint32_t run_q12(const OrdersColumns& orders,
                        const LineitemColumns& lineitem,
                        Q12Result& result,
                        cudaStream_t stream = 0)
{
    HashTable ht = allocate_hash_table(orders.num_rows, stream);

    GPU_CHECK(cudaMemsetAsync(result.count, 0, sizeof(uint32_t), stream));

    launchQuery(q12_kernel,
                stream,
                // Orders
                orders.o_orderkey,
                orders.o_orderpriority,
                orders.num_rows,
                // Lineitem
                lineitem.l_orderkey,
                lineitem.l_shipmode,
                lineitem.l_commitdate,
                lineitem.l_receiptdate,
                lineitem.l_shipdate,
                lineitem.num_rows,
                // Hash table
                ht.entries,
                ht.heads,
                ht.counter,
                ht.num_buckets,
                ht.capacity,
                // Output
                result.shipmode,
                result.orderpriority,
                result.count);

    uint32_t count;
    GPU_CHECK(cudaMemcpyAsync(&count, result.count, sizeof(uint32_t), cudaMemcpyDeviceToHost, stream));
    GPU_CHECK(cudaStreamSynchronize(stream));

    free_hash_table(ht);

    // ORDER BY l_shipmode
    if (count > 0) {
        // Allocate indices and sort
        int32_t* d_indices;
        GPU_CHECK(cudaMalloc(&d_indices, count * sizeof(int32_t)));
        sort_indices_by_key(result.shipmode, d_indices, count, stream);

        // Reorder all columns using indices
        reorder_by_indices(d_indices, count, stream, result.shipmode, result.orderpriority);

        GPU_CHECK(cudaFree(d_indices));
    }

    return count;
}

} // namespace gpu_native
