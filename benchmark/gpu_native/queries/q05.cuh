#pragma once

#include "../common/types.hpp"

#include "../common/hash_table.cuh"
#include "../common/launcher.cuh"

#include <cooperative_groups.h>

namespace cg = cooperative_groups;

namespace gpu_native {

// ============================================================================
// Q05: Six-way Join with Region Filter
// ============================================================================
// SELECT n_name, l_extendedprice, l_discount
// FROM customer, orders, lineitem, supplier, nation, region
// WHERE c_custkey = o_custkey
//   AND l_orderkey = o_orderkey
//   AND l_suppkey = s_suppkey
//   AND c_nationkey = s_nationkey
//   AND s_nationkey = n_nationkey
//   AND n_regionkey = r_regionkey
//   AND r_name = 'ASIA'
//   AND o_orderdate >= DATE '1994-01-01'
//   AND o_orderdate < DATE '1995-01-01';
// ============================================================================

struct Q05Result {
    int32_t* nationkey; // Use nationkey instead of n_name string
    double* extendedprice;
    double* discount;
    uint32_t* count;
    size_t capacity;
};

__global__ void q05_kernel(
    // Region table
    const int32_t* __restrict__ r_regionkey,
    const int32_t* __restrict__ r_name,
    size_t n_region,
    // Nation table
    const int32_t* __restrict__ n_nationkey,
    const int32_t* __restrict__ n_regionkey,
    size_t n_nation,
    // Supplier table
    const int32_t* __restrict__ s_suppkey,
    const int32_t* __restrict__ s_nationkey,
    size_t n_supplier,
    // Customer table
    const int32_t* __restrict__ c_custkey,
    const int32_t* __restrict__ c_nationkey,
    size_t n_customer,
    // Orders table
    const int32_t* __restrict__ o_orderkey,
    const int32_t* __restrict__ o_custkey,
    const int32_t* __restrict__ o_orderdate,
    size_t n_orders,
    // Lineitem table
    const int32_t* __restrict__ l_orderkey,
    const int32_t* __restrict__ l_suppkey,
    const double* __restrict__ l_extendedprice,
    const double* __restrict__ l_discount,
    size_t n_lineitem,
    // Hash table 1: Nation (nationkey -> nationkey for Asian nations)
    HashEntry* __restrict__ ht_nation_entries,
    uint32_t* __restrict__ ht_nation_heads,
    uint32_t* __restrict__ ht_nation_counter,
    uint32_t ht_nation_num_buckets,
    uint32_t ht_nation_capacity,
    // Hash table 2: Supplier (suppkey -> nationkey)
    HashEntry* __restrict__ ht_supplier_entries,
    uint32_t* __restrict__ ht_supplier_heads,
    uint32_t* __restrict__ ht_supplier_counter,
    uint32_t ht_supplier_num_buckets,
    uint32_t ht_supplier_capacity,
    // Hash table 3: Customer (custkey -> nationkey) for Asian customers
    HashEntry* __restrict__ ht_customer_entries,
    uint32_t* __restrict__ ht_customer_heads,
    uint32_t* __restrict__ ht_customer_counter,
    uint32_t ht_customer_num_buckets,
    uint32_t ht_customer_capacity,
    // Hash table 4: Orders (orderkey -> custkey) for date-filtered orders
    HashEntry* __restrict__ ht_orders_entries,
    uint32_t* __restrict__ ht_orders_heads,
    uint32_t* __restrict__ ht_orders_counter,
    uint32_t ht_orders_num_buckets,
    uint32_t ht_orders_capacity,
    // Intermediate: customer nationkey lookup
    int32_t* __restrict__ order_custkey_to_nationkey,
    // Output
    int32_t* __restrict__ out_nationkey,
    double* __restrict__ out_extendedprice,
    double* __restrict__ out_discount,
    uint32_t* __restrict__ result_count)
{
    cg::grid_group grid = cg::this_grid();

    constexpr int32_t REGION_ASIA = 2; // ASIA encoded
    constexpr int32_t DATE_START = 19940101;
    constexpr int32_t DATE_END = 19950101;

    // ========== PHASE 1: Find Asian region, build nation hash ==========
    // Find which regionkey is ASIA
    __shared__ int32_t asia_regionkey;
    if (threadIdx.x == 0) {
        asia_regionkey = -1;
    }
    __syncthreads();

    for (size_t i = grid.thread_rank(); i < n_region; i += grid.size()) {
        if (r_name[i] == REGION_ASIA) {
            asia_regionkey = r_regionkey[i];
        }
    }

    grid.sync(); // ===== SYNC 1 =====

    // Build hash table for Asian nations (nationkey -> nationkey)
    for (size_t i = grid.thread_rank(); i < n_nation; i += grid.size()) {
        if (n_regionkey[i] == asia_regionkey) {
            hash_table_insert(ht_nation_entries,
                              ht_nation_heads,
                              ht_nation_counter,
                              ht_nation_num_buckets,
                              ht_nation_capacity,
                              n_nationkey[i],
                              n_nationkey[i]);
        }
    }

    grid.sync(); // ===== SYNC 2 =====

    // ========== PHASE 2: Build supplier hash (Asian suppliers only) ==========
    for (size_t i = grid.thread_rank(); i < n_supplier; i += grid.size()) {
        int32_t nationkey = s_nationkey[i];
        if (hash_table_exists(ht_nation_entries, ht_nation_heads, ht_nation_num_buckets, nationkey)) {
            hash_table_insert(ht_supplier_entries,
                              ht_supplier_heads,
                              ht_supplier_counter,
                              ht_supplier_num_buckets,
                              ht_supplier_capacity,
                              s_suppkey[i],
                              nationkey);
        }
    }

    grid.sync(); // ===== SYNC 3 =====

    // ========== PHASE 3: Build customer hash (Asian customers only) ==========
    for (size_t i = grid.thread_rank(); i < n_customer; i += grid.size()) {
        int32_t nationkey = c_nationkey[i];
        if (hash_table_exists(ht_nation_entries, ht_nation_heads, ht_nation_num_buckets, nationkey)) {
            hash_table_insert(ht_customer_entries,
                              ht_customer_heads,
                              ht_customer_counter,
                              ht_customer_num_buckets,
                              ht_customer_capacity,
                              c_custkey[i],
                              nationkey);
        }
    }

    grid.sync(); // ===== SYNC 4 =====

    // ========== PHASE 4: Filter orders by date, join with customer ==========
    for (size_t i = grid.thread_rank(); i < n_orders; i += grid.size()) {
        int32_t orderdate = o_orderdate[i];

        if (orderdate >= DATE_START && orderdate < DATE_END) {
            int32_t custkey = o_custkey[i];
            int32_t cust_nationkey
                = hash_table_probe(ht_customer_entries, ht_customer_heads, ht_customer_num_buckets, custkey);

            if (cust_nationkey >= 0) {
                // Store order with customer's nationkey
                hash_table_insert(ht_orders_entries,
                                  ht_orders_heads,
                                  ht_orders_counter,
                                  ht_orders_num_buckets,
                                  ht_orders_capacity,
                                  o_orderkey[i],
                                  cust_nationkey);
            }
        }
    }

    grid.sync(); // ===== SYNC 5 =====

    // ========== PHASE 5: Join lineitem with orders and supplier ==========
    for (size_t i = grid.thread_rank(); i < n_lineitem; i += grid.size()) {
        int32_t orderkey = l_orderkey[i];
        int32_t suppkey = l_suppkey[i];

        // Get customer's nationkey from order
        int32_t cust_nationkey = hash_table_probe(ht_orders_entries, ht_orders_heads, ht_orders_num_buckets, orderkey);

        if (cust_nationkey >= 0) {
            // Get supplier's nationkey
            int32_t supp_nationkey
                = hash_table_probe(ht_supplier_entries, ht_supplier_heads, ht_supplier_num_buckets, suppkey);

            // Check if supplier and customer are in the same nation
            if (supp_nationkey == cust_nationkey) {
                uint32_t pos = atomicAdd(result_count, 1);
                out_nationkey[pos] = cust_nationkey;
                out_extendedprice[pos] = l_extendedprice[i];
                out_discount[pos] = l_discount[i];
            }
        }
    }
}

inline Q05Result allocate_q05_result(size_t capacity, cudaStream_t stream = 0)
{
    Q05Result result;
    result.capacity = capacity;

    GPU_CHECK(cudaMalloc(&result.nationkey, capacity * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&result.extendedprice, capacity * sizeof(double)));
    GPU_CHECK(cudaMalloc(&result.discount, capacity * sizeof(double)));
    GPU_CHECK(cudaMalloc(&result.count, sizeof(uint32_t)));
    GPU_CHECK(cudaMemsetAsync(result.count, 0, sizeof(uint32_t), stream));

    return result;
}

inline void free_q05_result(Q05Result& result)
{
    cudaFree(result.nationkey);
    cudaFree(result.extendedprice);
    cudaFree(result.discount);
    cudaFree(result.count);
}

inline uint32_t run_q05(const RegionColumns& region,
                        const NationColumns& nation,
                        const SupplierColumns& supplier,
                        const CustomerColumns& customer,
                        const OrdersColumns& orders,
                        const LineitemColumns& lineitem,
                        Q05Result& result,
                        cudaStream_t stream = 0)
{
    // Allocate hash tables
    HashTable ht_nation = allocate_hash_table(nation.num_rows, stream);
    HashTable ht_supplier = allocate_hash_table(supplier.num_rows, stream);
    HashTable ht_customer = allocate_hash_table(customer.num_rows, stream);
    HashTable ht_orders = allocate_hash_table(orders.num_rows, stream);

    // Dummy intermediate buffer
    int32_t* dummy;
    GPU_CHECK(cudaMalloc(&dummy, sizeof(int32_t)));

    // Reset result count
    GPU_CHECK(cudaMemsetAsync(result.count, 0, sizeof(uint32_t), stream));

    launchQuery(q05_kernel,
                stream,
                // Region
                region.r_regionkey,
                region.r_name,
                region.num_rows,
                // Nation
                nation.n_nationkey,
                nation.n_regionkey,
                nation.num_rows,
                // Supplier
                supplier.s_suppkey,
                supplier.s_nationkey,
                supplier.num_rows,
                // Customer
                customer.c_custkey,
                customer.c_nationkey,
                customer.num_rows,
                // Orders
                orders.o_orderkey,
                orders.o_custkey,
                orders.o_orderdate,
                orders.num_rows,
                // Lineitem
                lineitem.l_orderkey,
                lineitem.l_suppkey,
                lineitem.l_extendedprice,
                lineitem.l_discount,
                lineitem.num_rows,
                // Hash tables
                ht_nation.entries,
                ht_nation.heads,
                ht_nation.counter,
                ht_nation.num_buckets,
                ht_nation.capacity,
                ht_supplier.entries,
                ht_supplier.heads,
                ht_supplier.counter,
                ht_supplier.num_buckets,
                ht_supplier.capacity,
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
                // Intermediate
                dummy,
                // Output
                result.nationkey,
                result.extendedprice,
                result.discount,
                result.count);

    uint32_t count;
    GPU_CHECK(cudaMemcpyAsync(&count, result.count, sizeof(uint32_t), cudaMemcpyDeviceToHost, stream));
    GPU_CHECK(cudaStreamSynchronize(stream));

    // Cleanup
    cudaFree(dummy);
    free_hash_table(ht_nation);
    free_hash_table(ht_supplier);
    free_hash_table(ht_customer);
    free_hash_table(ht_orders);

    return count;
}

} // namespace gpu_native
