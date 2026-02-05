#pragma once

#include "../common/types.hpp"

#include "../common/hash_table.cuh"
#include "../common/launcher.cuh"
#include "../common/sort_utils.cuh"

#include <cooperative_groups.h>
#include <thrust/device_ptr.h>

namespace cg = cooperative_groups;

namespace gpu_native {

// ============================================================================
// Q10: Four-way Join with Return Flag Filter
// ============================================================================
// SELECT c_custkey, c_name, l_extendedprice, l_discount, c_acctbal,
//        n_name, c_address, c_phone, c_comment
// FROM nation, customer, orders, lineitem
// WHERE c_nationkey = n_nationkey
//   AND o_custkey = c_custkey
//   AND l_orderkey = o_orderkey
//   AND o_orderdate >= DATE '1993-10-01'
//   AND o_orderdate < DATE '1994-01-01'
//   AND l_returnflag = 'R'
// LIMIT 20;
// ============================================================================

struct Q10Result {
    int32_t* custkey;
    double* extendedprice;
    double* discount;
    double* acctbal;
    int32_t* nationkey;
    uint32_t* count;
    size_t capacity;
};

__global__ void q10_kernel(
    // Nation table
    const int32_t* __restrict__ n_nationkey,
    size_t n_nation,
    // Customer table
    const int32_t* __restrict__ c_custkey,
    const int32_t* __restrict__ c_nationkey,
    const double* __restrict__ c_acctbal,
    size_t n_customer,
    // Orders table
    const int32_t* __restrict__ o_orderkey,
    const int32_t* __restrict__ o_custkey,
    const int32_t* __restrict__ o_orderdate,
    size_t n_orders,
    // Lineitem table
    const int32_t* __restrict__ l_orderkey,
    const double* __restrict__ l_extendedprice,
    const double* __restrict__ l_discount,
    const int8_t* __restrict__ l_returnflag,
    size_t n_lineitem,
    // Hash table 1: Customer (custkey -> row index)
    HashEntry* __restrict__ ht_customer_entries,
    uint32_t* __restrict__ ht_customer_heads,
    uint32_t* __restrict__ ht_customer_counter,
    uint32_t ht_customer_num_buckets,
    uint32_t ht_customer_capacity,
    // Hash table 2: Orders (orderkey -> custkey)
    HashEntry* __restrict__ ht_orders_entries,
    uint32_t* __restrict__ ht_orders_heads,
    uint32_t* __restrict__ ht_orders_counter,
    uint32_t ht_orders_num_buckets,
    uint32_t ht_orders_capacity,
    // Output
    int32_t* __restrict__ out_custkey,
    double* __restrict__ out_extendedprice,
    double* __restrict__ out_discount,
    double* __restrict__ out_acctbal,
    int32_t* __restrict__ out_nationkey,
    uint32_t* __restrict__ result_count)
{
    cg::grid_group grid = cg::this_grid();

    constexpr int32_t DATE_START = 19931001; // 1993-10-01
    constexpr int32_t DATE_END = 19940101; // 1994-01-01
    constexpr int8_t RETURNFLAG_R = 'R';

    // ========== PHASE 1: Build customer hash table ==========
    for (size_t i = grid.thread_rank(); i < n_customer; i += grid.size()) {
        hash_table_insert(ht_customer_entries,
                          ht_customer_heads,
                          ht_customer_counter,
                          ht_customer_num_buckets,
                          ht_customer_capacity,
                          c_custkey[i],
                          static_cast<int32_t>(i));
    }

    grid.sync(); // ===== SYNC 1 =====

    // ========== PHASE 2: Filter orders by date, build hash table ==========
    for (size_t i = grid.thread_rank(); i < n_orders; i += grid.size()) {
        int32_t orderdate = o_orderdate[i];

        if (orderdate >= DATE_START && orderdate < DATE_END) {
            hash_table_insert(ht_orders_entries,
                              ht_orders_heads,
                              ht_orders_counter,
                              ht_orders_num_buckets,
                              ht_orders_capacity,
                              o_orderkey[i],
                              o_custkey[i]);
        }
    }

    grid.sync(); // ===== SYNC 2 =====

    // ========== PHASE 3: Join lineitem with orders and customer ==========
    for (size_t i = grid.thread_rank(); i < n_lineitem; i += grid.size()) {
        if (l_returnflag[i] == RETURNFLAG_R) {
            int32_t orderkey = l_orderkey[i];

            // Get custkey from orders
            int32_t custkey = hash_table_probe(ht_orders_entries, ht_orders_heads, ht_orders_num_buckets, orderkey);

            if (custkey >= 0) {
                // Get customer row
                int32_t cust_row
                    = hash_table_probe(ht_customer_entries, ht_customer_heads, ht_customer_num_buckets, custkey);

                if (cust_row >= 0) {
                    uint32_t pos = atomicAdd(result_count, 1);
                    out_custkey[pos] = custkey;
                    out_extendedprice[pos] = l_extendedprice[i];
                    out_discount[pos] = l_discount[i];
                    out_acctbal[pos] = c_acctbal[cust_row];
                    out_nationkey[pos] = c_nationkey[cust_row];
                }
            }
        }
    }
}

inline Q10Result allocate_q10_result(size_t capacity, cudaStream_t stream = 0)
{
    Q10Result result;
    result.capacity = capacity;

    GPU_CHECK(cudaMalloc(&result.custkey, capacity * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&result.extendedprice, capacity * sizeof(double)));
    GPU_CHECK(cudaMalloc(&result.discount, capacity * sizeof(double)));
    GPU_CHECK(cudaMalloc(&result.acctbal, capacity * sizeof(double)));
    GPU_CHECK(cudaMalloc(&result.nationkey, capacity * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&result.count, sizeof(uint32_t)));
    GPU_CHECK(cudaMemsetAsync(result.count, 0, sizeof(uint32_t), stream));

    return result;
}

inline void free_q10_result(Q10Result& result)
{
    cudaFree(result.custkey);
    cudaFree(result.extendedprice);
    cudaFree(result.discount);
    cudaFree(result.acctbal);
    cudaFree(result.nationkey);
    cudaFree(result.count);
}

inline uint32_t run_q10(const NationColumns& nation,
                        const CustomerColumns& customer,
                        const OrdersColumns& orders,
                        const LineitemColumns& lineitem,
                        Q10Result& result,
                        cudaStream_t stream = 0)
{
    HashTable ht_customer = allocate_hash_table(customer.num_rows, stream);
    HashTable ht_orders = allocate_hash_table(orders.num_rows, stream);

    GPU_CHECK(cudaMemsetAsync(result.count, 0, sizeof(uint32_t), stream));

    launchQuery(q10_kernel,
                stream,
                // Nation
                nation.n_nationkey,
                nation.num_rows,
                // Customer
                customer.c_custkey,
                customer.c_nationkey,
                customer.c_acctbal,
                customer.num_rows,
                // Orders
                orders.o_orderkey,
                orders.o_custkey,
                orders.o_orderdate,
                orders.num_rows,
                // Lineitem
                lineitem.l_orderkey,
                lineitem.l_extendedprice,
                lineitem.l_discount,
                lineitem.l_returnflag,
                lineitem.num_rows,
                // Hash tables
                ht_customer.entries,
                ht_customer.heads,
                ht_customer.counter,
                ht_customer.num_buckets,
                ht_customer.capacity,
                ht_orders.entries,
                ht_orders.heads,
                ht_orders.counter,
                ht_orders.num_buckets,
                ht_orders.capacity,
                // Output
                result.custkey,
                result.extendedprice,
                result.discount,
                result.acctbal,
                result.nationkey,
                result.count);

    uint32_t count;
    GPU_CHECK(cudaMemcpyAsync(&count, result.count, sizeof(uint32_t), cudaMemcpyDeviceToHost, stream));
    GPU_CHECK(cudaStreamSynchronize(stream));

    free_hash_table(ht_customer);
    free_hash_table(ht_orders);

    // LIMIT 20
    return std::min(count, 20u);
}

} // namespace gpu_native
