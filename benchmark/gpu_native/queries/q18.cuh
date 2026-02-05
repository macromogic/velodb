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
// Q18: Three-way Join (Customer - Orders - Lineitem)
// ============================================================================
// SELECT c_name, c_custkey, o_orderkey, o_orderdate, o_totalprice, l_quantity
// FROM customer, orders, lineitem
// WHERE o_custkey = c_custkey
//   AND l_orderkey = o_orderkey
// ORDER BY o_totalprice DESC, o_orderdate
// LIMIT 100;
// ============================================================================

struct Q18Result {
    int32_t* custkey;
    int32_t* orderkey;
    int32_t* orderdate;
    double* totalprice;
    double* quantity;
    uint32_t* count;
    size_t capacity;
};

__global__ void q18_kernel(
    // Customer table
    const int32_t* __restrict__ c_custkey,
    size_t n_customer,
    // Orders table
    const int32_t* __restrict__ o_orderkey,
    const int32_t* __restrict__ o_custkey,
    const int32_t* __restrict__ o_orderdate,
    const double* __restrict__ o_totalprice,
    size_t n_orders,
    // Lineitem table
    const int32_t* __restrict__ l_orderkey,
    const double* __restrict__ l_quantity,
    size_t n_lineitem,
    // Hash table 1: Customer (custkey -> row)
    HashEntry* __restrict__ ht_customer_entries,
    uint32_t* __restrict__ ht_customer_heads,
    uint32_t* __restrict__ ht_customer_counter,
    uint32_t ht_customer_num_buckets,
    uint32_t ht_customer_capacity,
    // Hash table 2: Orders (orderkey -> row)
    HashEntry* __restrict__ ht_orders_entries,
    uint32_t* __restrict__ ht_orders_heads,
    uint32_t* __restrict__ ht_orders_counter,
    uint32_t ht_orders_num_buckets,
    uint32_t ht_orders_capacity,
    // Output
    int32_t* __restrict__ out_custkey,
    int32_t* __restrict__ out_orderkey,
    int32_t* __restrict__ out_orderdate,
    double* __restrict__ out_totalprice,
    double* __restrict__ out_quantity,
    uint32_t* __restrict__ result_count)
{
    cg::grid_group grid = cg::this_grid();

    // ========== PHASE 1: Build hash table on customer ==========
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

    // ========== PHASE 2: Join orders with customer, build orders hash table ==========
    for (size_t i = grid.thread_rank(); i < n_orders; i += grid.size()) {
        int32_t custkey = o_custkey[i];

        // Check if customer exists
        if (hash_table_exists(ht_customer_entries, ht_customer_heads, ht_customer_num_buckets, custkey)) {
            hash_table_insert(ht_orders_entries,
                              ht_orders_heads,
                              ht_orders_counter,
                              ht_orders_num_buckets,
                              ht_orders_capacity,
                              o_orderkey[i],
                              static_cast<int32_t>(i));
        }
    }

    grid.sync(); // ===== SYNC 2 =====

    // ========== PHASE 3: Probe lineitem and output ==========
    for (size_t i = grid.thread_rank(); i < n_lineitem; i += grid.size()) {
        int32_t orderkey = l_orderkey[i];

        // Probe orders hash table
        int32_t order_row = hash_table_probe(ht_orders_entries, ht_orders_heads, ht_orders_num_buckets, orderkey);

        if (order_row >= 0) {
            int32_t custkey = o_custkey[order_row];

            uint32_t pos = atomicAdd(result_count, 1);
            out_custkey[pos] = custkey;
            out_orderkey[pos] = orderkey;
            out_orderdate[pos] = o_orderdate[order_row];
            out_totalprice[pos] = o_totalprice[order_row];
            out_quantity[pos] = l_quantity[i];
        }
    }
}

inline Q18Result allocate_q18_result(size_t capacity, cudaStream_t stream = 0)
{
    Q18Result result;
    result.capacity = capacity;

    GPU_CHECK(cudaMalloc(&result.custkey, capacity * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&result.orderkey, capacity * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&result.orderdate, capacity * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&result.totalprice, capacity * sizeof(double)));
    GPU_CHECK(cudaMalloc(&result.quantity, capacity * sizeof(double)));
    GPU_CHECK(cudaMalloc(&result.count, sizeof(uint32_t)));
    GPU_CHECK(cudaMemsetAsync(result.count, 0, sizeof(uint32_t), stream));

    return result;
}

inline void free_q18_result(Q18Result& result)
{
    cudaFree(result.custkey);
    cudaFree(result.orderkey);
    cudaFree(result.orderdate);
    cudaFree(result.totalprice);
    cudaFree(result.quantity);
    cudaFree(result.count);
}

inline uint32_t run_q18(const CustomerColumns& customer,
                        const OrdersColumns& orders,
                        const LineitemColumns& lineitem,
                        Q18Result& result,
                        cudaStream_t stream = 0)
{
    HashTable ht_customer = allocate_hash_table(customer.num_rows, stream);
    HashTable ht_orders = allocate_hash_table(orders.num_rows, stream);

    GPU_CHECK(cudaMemsetAsync(result.count, 0, sizeof(uint32_t), stream));

    launchQuery(q18_kernel,
                stream,
                // Customer
                customer.c_custkey,
                customer.num_rows,
                // Orders
                orders.o_orderkey,
                orders.o_custkey,
                orders.o_orderdate,
                orders.o_totalprice,
                orders.num_rows,
                // Lineitem
                lineitem.l_orderkey,
                lineitem.l_quantity,
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
                result.orderkey,
                result.orderdate,
                result.totalprice,
                result.quantity,
                result.count);

    uint32_t count;
    GPU_CHECK(cudaMemcpyAsync(&count, result.count, sizeof(uint32_t), cudaMemcpyDeviceToHost, stream));
    GPU_CHECK(cudaStreamSynchronize(stream));

    free_hash_table(ht_customer);
    free_hash_table(ht_orders);

    // ORDER BY o_totalprice DESC, o_orderdate
    if (count > 0) {
        sort_q18_results(result.totalprice,
                         result.orderdate,
                         result.custkey,
                         result.orderkey,
                         result.quantity,
                         count,
                         stream);
    }

    // LIMIT 100
    return std::min(count, 100u);
}

} // namespace gpu_native
