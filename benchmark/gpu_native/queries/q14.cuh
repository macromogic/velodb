#pragma once

#include "../common/types.hpp"

#include "../common/hash_table.cuh"
#include "../common/launcher.cuh"

#include <cooperative_groups.h>

namespace cg = cooperative_groups;

namespace gpu_native {

// ============================================================================
// Q14: Lineitem-Part Join with Date Filter
// ============================================================================
// SELECT p_type, l_extendedprice, l_discount
// FROM lineitem JOIN part ON l_partkey = p_partkey
// WHERE l_shipdate >= DATE '1995-09-01'
//   AND l_shipdate < DATE '1995-10-01';
// ============================================================================

struct Q14Result {
    int32_t* partkey; // Use partkey as proxy for p_type lookup
    double* extendedprice;
    double* discount;
    uint32_t* count;
    size_t capacity;
};

__global__ void q14_kernel(
    // Lineitem table (build side with filter)
    const int32_t* __restrict__ l_partkey,
    const double* __restrict__ l_extendedprice,
    const double* __restrict__ l_discount,
    const int32_t* __restrict__ l_shipdate,
    size_t n_lineitem,
    // Part table (probe side)
    const int32_t* __restrict__ p_partkey,
    size_t n_part,
    // Hash table: Lineitem (partkey -> rowid)
    HashEntry* __restrict__ ht_entries,
    uint32_t* __restrict__ ht_heads,
    uint32_t* __restrict__ ht_counter,
    uint32_t ht_num_buckets,
    uint32_t ht_capacity,
    // Intermediate storage for filtered lineitem data
    int32_t* __restrict__ filtered_partkey,
    double* __restrict__ filtered_extendedprice,
    double* __restrict__ filtered_discount,
    uint32_t* __restrict__ filtered_count,
    // Output
    int32_t* __restrict__ out_partkey,
    double* __restrict__ out_extendedprice,
    double* __restrict__ out_discount,
    uint32_t* __restrict__ result_count)
{
    cg::grid_group grid = cg::this_grid();

    constexpr int32_t DATE_START = 19950901; // 1995-09-01
    constexpr int32_t DATE_END = 19951001; // 1995-10-01

    // ========== PHASE 1: Filter lineitem and build hash table ==========
    for (size_t i = grid.thread_rank(); i < n_lineitem; i += grid.size()) {
        int32_t shipdate = l_shipdate[i];

        if (shipdate >= DATE_START && shipdate < DATE_END) {
            uint32_t pos = atomicAdd(filtered_count, 1);
            filtered_partkey[pos] = l_partkey[i];
            filtered_extendedprice[pos] = l_extendedprice[i];
            filtered_discount[pos] = l_discount[i];

            hash_table_insert(ht_entries,
                              ht_heads,
                              ht_counter,
                              ht_num_buckets,
                              ht_capacity,
                              l_partkey[i],
                              static_cast<int32_t>(pos));
        }
    }

    grid.sync(); // ===== SYNC =====

    // ========== PHASE 2: Probe part into lineitem hash table ==========
    for (size_t i = grid.thread_rank(); i < n_part; i += grid.size()) {
        int32_t partkey = p_partkey[i];

        // Probe all matching lineitem rows for this partkey
        uint32_t bucket = murmurhash32(partkey) % ht_num_buckets;
        uint32_t idx = ht_heads[bucket];

        while (idx != UINT32_MAX) {
            if (ht_entries[idx].key == partkey) {
                int32_t lineitem_row = ht_entries[idx].rowid;
                uint32_t pos = atomicAdd(result_count, 1);
                out_partkey[pos] = partkey;
                out_extendedprice[pos] = filtered_extendedprice[lineitem_row];
                out_discount[pos] = filtered_discount[lineitem_row];
            }
            idx = ht_entries[idx].next;
        }
    }
}

inline Q14Result allocate_q14_result(size_t capacity, cudaStream_t stream = 0)
{
    Q14Result result;
    result.capacity = capacity;

    GPU_CHECK(cudaMalloc(&result.partkey, capacity * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&result.extendedprice, capacity * sizeof(double)));
    GPU_CHECK(cudaMalloc(&result.discount, capacity * sizeof(double)));
    GPU_CHECK(cudaMalloc(&result.count, sizeof(uint32_t)));
    GPU_CHECK(cudaMemsetAsync(result.count, 0, sizeof(uint32_t), stream));

    return result;
}

inline void free_q14_result(Q14Result& result)
{
    cudaFree(result.partkey);
    cudaFree(result.extendedprice);
    cudaFree(result.discount);
    cudaFree(result.count);
}

inline uint32_t run_q14(const PartColumns& part,
                        const LineitemColumns& lineitem,
                        Q14Result& result,
                        cudaStream_t stream = 0)
{
    // Hash table for filtered lineitem rows (build side)
    HashTable ht = allocate_hash_table(lineitem.num_rows, stream);

    // Intermediate storage for filtered lineitem data
    int32_t* filtered_partkey;
    double* filtered_extendedprice;
    double* filtered_discount;
    uint32_t* filtered_count;
    GPU_CHECK(cudaMalloc(&filtered_partkey, lineitem.num_rows * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&filtered_extendedprice, lineitem.num_rows * sizeof(double)));
    GPU_CHECK(cudaMalloc(&filtered_discount, lineitem.num_rows * sizeof(double)));
    GPU_CHECK(cudaMalloc(&filtered_count, sizeof(uint32_t)));
    GPU_CHECK(cudaMemsetAsync(filtered_count, 0, sizeof(uint32_t), stream));

    GPU_CHECK(cudaMemsetAsync(result.count, 0, sizeof(uint32_t), stream));

    launchQuery(q14_kernel,
                stream,
                // Lineitem (build side)
                lineitem.l_partkey,
                lineitem.l_extendedprice,
                lineitem.l_discount,
                lineitem.l_shipdate,
                lineitem.num_rows,
                // Part (probe side)
                part.p_partkey,
                part.num_rows,
                // Hash table
                ht.entries,
                ht.heads,
                ht.counter,
                ht.num_buckets,
                ht.capacity,
                // Filtered storage
                filtered_partkey,
                filtered_extendedprice,
                filtered_discount,
                filtered_count,
                // Output
                result.partkey,
                result.extendedprice,
                result.discount,
                result.count);

    uint32_t count;
    GPU_CHECK(cudaMemcpyAsync(&count, result.count, sizeof(uint32_t), cudaMemcpyDeviceToHost, stream));
    GPU_CHECK(cudaStreamSynchronize(stream));

    // Cleanup
    GPU_CHECK(cudaFree(filtered_partkey));
    GPU_CHECK(cudaFree(filtered_extendedprice));
    GPU_CHECK(cudaFree(filtered_discount));
    GPU_CHECK(cudaFree(filtered_count));
    free_hash_table(ht);

    return count;
}

} // namespace gpu_native
