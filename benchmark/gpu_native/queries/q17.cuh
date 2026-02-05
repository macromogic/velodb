#pragma once

#include "../common/types.hpp"

#include "../common/hash_table.cuh"
#include "../common/launcher.cuh"

#include <cooperative_groups.h>

namespace cg = cooperative_groups;

namespace gpu_native {

// ============================================================================
// Q17: Lineitem-Part Join with Brand/Container Filter
// ============================================================================
// SELECT l_extendedprice
// FROM lineitem JOIN part ON p_partkey = l_partkey
// WHERE p_brand = 'Brand#23'
//   AND p_container = 'MED BOX';
// ============================================================================

struct Q17Result {
    double* extendedprice;
    uint32_t* count;
    size_t capacity;
};

__global__ void q17_kernel(
    // Lineitem table (build side)
    const int32_t* __restrict__ l_partkey,
    const double* __restrict__ l_extendedprice,
    size_t n_lineitem,
    // Part table (probe side with filter)
    const int32_t* __restrict__ p_partkey,
    const int32_t* __restrict__ p_brand,
    const int32_t* __restrict__ p_container,
    size_t n_part,
    // Hash table: Lineitem (partkey -> rowid)
    HashEntry* __restrict__ ht_entries,
    uint32_t* __restrict__ ht_heads,
    uint32_t* __restrict__ ht_counter,
    uint32_t ht_num_buckets,
    uint32_t ht_capacity,
    // Output
    double* __restrict__ out_extendedprice,
    uint32_t* __restrict__ result_count)
{
    cg::grid_group grid = cg::this_grid();

    constexpr int32_t BRAND_23 = 23; // Brand#23
    constexpr int32_t CONTAINER_MED_BOX = 5; // MED BOX from encoding

    // ========== PHASE 1: Build hash table on lineitem ==========
    for (size_t i = grid.thread_rank(); i < n_lineitem; i += grid.size()) {
        hash_table_insert(ht_entries,
                          ht_heads,
                          ht_counter,
                          ht_num_buckets,
                          ht_capacity,
                          l_partkey[i],
                          static_cast<int32_t>(i));
    }

    grid.sync(); // ===== SYNC =====

    // ========== PHASE 2: Filter part and probe lineitem hash table ==========
    for (size_t i = grid.thread_rank(); i < n_part; i += grid.size()) {
        if (p_brand[i] == BRAND_23 && p_container[i] == CONTAINER_MED_BOX) {
            int32_t partkey = p_partkey[i];

            // Probe all matching lineitem rows
            uint32_t bucket = murmurhash32(partkey) % ht_num_buckets;
            uint32_t idx = ht_heads[bucket];

            while (idx != UINT32_MAX) {
                if (ht_entries[idx].key == partkey) {
                    int32_t lineitem_row = ht_entries[idx].rowid;
                    uint32_t pos = atomicAdd(result_count, 1);
                    out_extendedprice[pos] = l_extendedprice[lineitem_row];
                }
                idx = ht_entries[idx].next;
            }
        }
    }
}

inline Q17Result allocate_q17_result(size_t capacity, cudaStream_t stream = 0)
{
    Q17Result result;
    result.capacity = capacity;

    GPU_CHECK(cudaMalloc(&result.extendedprice, capacity * sizeof(double)));
    GPU_CHECK(cudaMalloc(&result.count, sizeof(uint32_t)));
    GPU_CHECK(cudaMemsetAsync(result.count, 0, sizeof(uint32_t), stream));

    return result;
}

inline void free_q17_result(Q17Result& result)
{
    cudaFree(result.extendedprice);
    cudaFree(result.count);
}

inline uint32_t run_q17(const PartColumns& part,
                        const LineitemColumns& lineitem,
                        Q17Result& result,
                        cudaStream_t stream = 0)
{
    // Hash table for lineitem rows (build side)
    HashTable ht = allocate_hash_table(lineitem.num_rows, stream);

    GPU_CHECK(cudaMemsetAsync(result.count, 0, sizeof(uint32_t), stream));

    launchQuery(q17_kernel,
                stream,
                // Lineitem (build side)
                lineitem.l_partkey,
                lineitem.l_extendedprice,
                lineitem.num_rows,
                // Part (probe side)
                part.p_partkey,
                part.p_brand,
                part.p_container,
                part.num_rows,
                // Hash table
                ht.entries,
                ht.heads,
                ht.counter,
                ht.num_buckets,
                ht.capacity,
                // Output
                result.extendedprice,
                result.count);

    uint32_t count;
    GPU_CHECK(cudaMemcpyAsync(&count, result.count, sizeof(uint32_t), cudaMemcpyDeviceToHost, stream));
    GPU_CHECK(cudaStreamSynchronize(stream));

    free_hash_table(ht);

    return count;
}

} // namespace gpu_native
