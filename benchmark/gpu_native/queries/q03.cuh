#pragma once

#include "../common/types.hpp"

#include "../common/hash_table.cuh"
#include "../common/launcher.cuh"
#include "../common/sort_utils.cuh"

#include <cooperative_groups.h>

namespace cg = cooperative_groups;

namespace gpu_native {

// ============================================================================
// Q03: Three-way Join (Customer - Orders - Lineitem)
// ============================================================================
// SELECT l_orderkey, l_extendedprice, l_discount, o_orderdate, o_shippriority
// FROM customer, orders, lineitem
// WHERE c_mktsegment = 'BUILDING'
//   AND c_custkey = o_custkey
//   AND l_orderkey = o_orderkey
//   AND o_orderdate < DATE '1995-03-15'
//   AND l_shipdate > DATE '1995-03-15'
// ORDER BY o_orderdate
// LIMIT 10;
// ============================================================================

struct Q03Result {
    int32_t* orderkey;
    double* extendedprice;
    double* discount;
    int32_t* orderdate;
    int32_t* shippriority;
    uint32_t* count;
    size_t capacity;
};

// Intermediate buffer for filtered orders
struct Q03Intermediate {
    int32_t* orderkey;
    int32_t* orderdate;
    int32_t* shippriority;
    uint32_t* count;
    size_t capacity;
};

__global__ void q03_kernel(
    // Customer table
    const int32_t* __restrict__ c_custkey,
    const int32_t* __restrict__ c_mktsegment,
    size_t n_customer,
    // Orders table
    const int32_t* __restrict__ o_orderkey,
    const int32_t* __restrict__ o_custkey,
    const int32_t* __restrict__ o_orderdate,
    const int32_t* __restrict__ o_shippriority,
    size_t n_orders,
    // Lineitem table
    const int32_t* __restrict__ l_orderkey,
    const double* __restrict__ l_extendedprice,
    const double* __restrict__ l_discount,
    const int32_t* __restrict__ l_shipdate,
    size_t n_lineitem,
    // Hash table 1: Customer (build)
    HashEntry* __restrict__ ht1_entries,
    uint32_t* __restrict__ ht1_heads,
    uint32_t* __restrict__ ht1_counter,
    uint32_t ht1_num_buckets,
    uint32_t ht1_capacity,
    // Hash table 2: Orders (build after join with customer)
    HashEntry* __restrict__ ht2_entries,
    uint32_t* __restrict__ ht2_heads,
    uint32_t* __restrict__ ht2_counter,
    uint32_t ht2_num_buckets,
    uint32_t ht2_capacity,
    // Intermediate: filtered orders
    int32_t* __restrict__ filt_orderkey,
    int32_t* __restrict__ filt_orderdate,
    int32_t* __restrict__ filt_shippriority,
    uint32_t* __restrict__ filt_count,
    // Output
    int32_t* __restrict__ out_orderkey,
    double* __restrict__ out_extendedprice,
    double* __restrict__ out_discount,
    int32_t* __restrict__ out_orderdate,
    int32_t* __restrict__ out_shippriority,
    uint32_t* __restrict__ result_count)
{
    cg::grid_group grid = cg::this_grid();

    // Date constants
    constexpr int32_t ORDER_DATE_THRESHOLD = 19950315; // 1995-03-15
    constexpr int32_t MKTSEGMENT_BUILDING = 0;

    // ========== PHASE 1: Build hash table on filtered customer ==========
    for (size_t i = grid.thread_rank(); i < n_customer; i += grid.size()) {
        if (c_mktsegment[i] == MKTSEGMENT_BUILDING) {
            hash_table_insert(ht1_entries,
                              ht1_heads,
                              ht1_counter,
                              ht1_num_buckets,
                              ht1_capacity,
                              c_custkey[i],
                              static_cast<int32_t>(i));
        }
    }

    grid.sync(); // ===== SYNC 1 =====

    // ========== PHASE 2: Probe orders, filter, build HT2 ==========
    for (size_t i = grid.thread_rank(); i < n_orders; i += grid.size()) {
        int32_t orderdate = o_orderdate[i];

        if (orderdate < ORDER_DATE_THRESHOLD) {
            int32_t custkey = o_custkey[i];

            // Check if customer matches (exists in HT1)
            if (hash_table_exists(ht1_entries, ht1_heads, ht1_num_buckets, custkey)) {
                // Store filtered order
                uint32_t fpos = atomicAdd(filt_count, 1);
                filt_orderkey[fpos] = o_orderkey[i];
                filt_orderdate[fpos] = orderdate;
                filt_shippriority[fpos] = o_shippriority[i];

                // Build HT2 on orderkey -> fpos
                hash_table_insert(ht2_entries,
                                  ht2_heads,
                                  ht2_counter,
                                  ht2_num_buckets,
                                  ht2_capacity,
                                  o_orderkey[i],
                                  static_cast<int32_t>(fpos));
            }
        }
    }

    grid.sync(); // ===== SYNC 2 =====

    // ========== PHASE 3: Probe lineitem, produce output ==========
    for (size_t i = grid.thread_rank(); i < n_lineitem; i += grid.size()) {
        int32_t shipdate = l_shipdate[i];

        if (shipdate > ORDER_DATE_THRESHOLD) {
            int32_t orderkey = l_orderkey[i];

            // Probe HT2
            uint32_t bucket = murmurhash32(orderkey) % ht2_num_buckets;
            uint32_t curr = ht2_heads[bucket];

            while (curr != HASH_EMPTY) {
                if (ht2_entries[curr].key == orderkey) {
                    int32_t fpos = ht2_entries[curr].rowid;

                    // Output result
                    uint32_t out_pos = atomicAdd(result_count, 1);
                    out_orderkey[out_pos] = orderkey;
                    out_extendedprice[out_pos] = l_extendedprice[i];
                    out_discount[out_pos] = l_discount[i];
                    out_orderdate[out_pos] = filt_orderdate[fpos];
                    out_shippriority[out_pos] = filt_shippriority[fpos];
                }
                curr = ht2_entries[curr].next;
            }
        }
    }
}

inline Q03Result allocate_q03_result(size_t capacity, cudaStream_t stream = 0)
{
    Q03Result result;
    result.capacity = capacity;

    GPU_CHECK(cudaMalloc(&result.orderkey, capacity * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&result.extendedprice, capacity * sizeof(double)));
    GPU_CHECK(cudaMalloc(&result.discount, capacity * sizeof(double)));
    GPU_CHECK(cudaMalloc(&result.orderdate, capacity * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&result.shippriority, capacity * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&result.count, sizeof(uint32_t)));
    GPU_CHECK(cudaMemsetAsync(result.count, 0, sizeof(uint32_t), stream));

    return result;
}

inline Q03Intermediate allocate_q03_intermediate(size_t capacity, cudaStream_t stream = 0)
{
    Q03Intermediate inter;
    inter.capacity = capacity;

    GPU_CHECK(cudaMalloc(&inter.orderkey, capacity * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&inter.orderdate, capacity * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&inter.shippriority, capacity * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&inter.count, sizeof(uint32_t)));
    GPU_CHECK(cudaMemsetAsync(inter.count, 0, sizeof(uint32_t), stream));

    return inter;
}

inline void free_q03_result(Q03Result& result)
{
    cudaFree(result.orderkey);
    cudaFree(result.extendedprice);
    cudaFree(result.discount);
    cudaFree(result.orderdate);
    cudaFree(result.shippriority);
    cudaFree(result.count);
}

inline void free_q03_intermediate(Q03Intermediate& inter)
{
    cudaFree(inter.orderkey);
    cudaFree(inter.orderdate);
    cudaFree(inter.shippriority);
    cudaFree(inter.count);
}

inline uint32_t run_q03(const CustomerColumns& customer,
                        const OrdersColumns& orders,
                        const LineitemColumns& lineitem,
                        Q03Result& result,
                        cudaStream_t stream = 0)
{
    // Allocate hash tables
    HashTable ht1 = allocate_hash_table(customer.num_rows, stream);
    HashTable ht2 = allocate_hash_table(orders.num_rows, stream);

    // Allocate intermediate buffer
    Q03Intermediate inter = allocate_q03_intermediate(orders.num_rows, stream);

    // Reset result count
    GPU_CHECK(cudaMemsetAsync(result.count, 0, sizeof(uint32_t), stream));

    launchQuery(q03_kernel,
                stream,
                // Customer
                customer.c_custkey,
                customer.c_mktsegment,
                customer.num_rows,
                // Orders
                orders.o_orderkey,
                orders.o_custkey,
                orders.o_orderdate,
                orders.o_shippriority,
                orders.num_rows,
                // Lineitem
                lineitem.l_orderkey,
                lineitem.l_extendedprice,
                lineitem.l_discount,
                lineitem.l_shipdate,
                lineitem.num_rows,
                // HT1
                ht1.entries,
                ht1.heads,
                ht1.counter,
                ht1.num_buckets,
                ht1.capacity,
                // HT2
                ht2.entries,
                ht2.heads,
                ht2.counter,
                ht2.num_buckets,
                ht2.capacity,
                // Intermediate
                inter.orderkey,
                inter.orderdate,
                inter.shippriority,
                inter.count,
                // Output
                result.orderkey,
                result.extendedprice,
                result.discount,
                result.orderdate,
                result.shippriority,
                result.count);

    uint32_t count;
    GPU_CHECK(cudaMemcpyAsync(&count, result.count, sizeof(uint32_t), cudaMemcpyDeviceToHost, stream));
    GPU_CHECK(cudaStreamSynchronize(stream));

    // ORDER BY o_orderdate LIMIT 10
    if (count > 0) {
        constexpr size_t LIMIT = 10;
        sort_results_by_key(result.orderdate,
                            count,
                            count,
                            stream,
                            result.orderkey,
                            result.extendedprice,
                            result.discount,
                            result.shippriority);
        count = std::min(count, static_cast<uint32_t>(LIMIT));
    }

    // Cleanup
    free_hash_table(ht1);
    free_hash_table(ht2);
    free_q03_intermediate(inter);

    return count;
}

} // namespace gpu_native
