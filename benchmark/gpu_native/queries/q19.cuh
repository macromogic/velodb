#pragma once

#include "../common/types.hpp"

#include "../common/hash_table.cuh"
#include "../common/launcher.cuh"

#include <cooperative_groups.h>

namespace cg = cooperative_groups;

namespace gpu_native {

// ============================================================================
// Q19: Lineitem-Part Join with Complex Multi-Predicate Filter
// ============================================================================
// SELECT l_extendedprice, l_discount
// FROM lineitem JOIN part ON l_partkey = p_partkey
// WHERE (
//     p_brand = 'Brand#12'
//     AND p_container IN ('SM CASE', 'SM BOX', 'SM PACK', 'SM PKG')
//     AND l_quantity >= 1.0 AND l_quantity <= 11.0
//     AND p_size BETWEEN 1 AND 5
//     AND l_shipmode IN ('AIR', 'AIR REG')
//     AND l_shipinstruct = 'DELIVER IN PERSON'
//   );
// ============================================================================

struct Q19Result {
    double* extendedprice;
    double* discount;
    uint32_t* count;
    size_t capacity;
};

// Predicate helper: check if container is one of SM CASE, SM BOX, SM PACK, SM PKG
__device__ __forceinline__ bool is_sm_container(int32_t container)
{
    // SM CASE=0, SM BOX=1, SM PACK=2, SM PKG=3
    return container >= 0 && container <= 3;
}

// Predicate helper: check if shipmode is AIR or AIR REG
__device__ __forceinline__ bool is_air_shipmode(int32_t shipmode)
{
    // REG AIR=0, AIR=1
    return shipmode == 0 || shipmode == 1;
}

__global__ void q19_kernel(
    // Lineitem table (build side with filter)
    const int32_t* __restrict__ l_partkey,
    const double* __restrict__ l_extendedprice,
    const double* __restrict__ l_discount,
    const double* __restrict__ l_quantity,
    const int32_t* __restrict__ l_shipmode,
    const int32_t* __restrict__ l_shipinstruct,
    size_t n_lineitem,
    // Part table (probe side with filter)
    const int32_t* __restrict__ p_partkey,
    const int32_t* __restrict__ p_brand,
    const int32_t* __restrict__ p_container,
    const int32_t* __restrict__ p_size,
    size_t n_part,
    // Hash table: Lineitem (partkey -> rowid)
    HashEntry* __restrict__ ht_entries,
    uint32_t* __restrict__ ht_heads,
    uint32_t* __restrict__ ht_counter,
    uint32_t ht_num_buckets,
    uint32_t ht_capacity,
    // Intermediate storage for filtered lineitem
    double* __restrict__ filtered_extendedprice,
    double* __restrict__ filtered_discount,
    uint32_t* __restrict__ filtered_count,
    // Output
    double* __restrict__ out_extendedprice,
    double* __restrict__ out_discount,
    uint32_t* __restrict__ result_count)
{
    cg::grid_group grid = cg::this_grid();

    constexpr int32_t BRAND_12 = 12;
    constexpr int32_t SHIPINSTRUCT_DELIVER_IN_PERSON = 0;

    // ========== PHASE 1: Filter lineitem and build hash table ==========
    // Filter: l_quantity >= 1.0 AND l_quantity <= 11.0
    //         AND l_shipmode IN (AIR, REG AIR) AND l_shipinstruct = 'DELIVER IN PERSON'
    for (size_t i = grid.thread_rank(); i < n_lineitem; i += grid.size()) {
        double quantity = l_quantity[i];
        int32_t shipmode = l_shipmode[i];
        int32_t shipinstruct = l_shipinstruct[i];

        bool lineitem_pred = (quantity >= 1.0) && (quantity <= 11.0) && is_air_shipmode(shipmode)
            && (shipinstruct == SHIPINSTRUCT_DELIVER_IN_PERSON);

        if (lineitem_pred) {
            uint32_t pos = atomicAdd(filtered_count, 1);
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

    // ========== PHASE 2: Filter part and probe lineitem hash table ==========
    // Filter: p_brand = 'Brand#12' AND p_container IN (SM*) AND p_size BETWEEN 1 AND 5
    for (size_t i = grid.thread_rank(); i < n_part; i += grid.size()) {
        int32_t brand = p_brand[i];
        int32_t container = p_container[i];
        int32_t size = p_size[i];

        if (brand == BRAND_12 && is_sm_container(container) && size >= 1 && size <= 5) {
            int32_t partkey = p_partkey[i];

            // Probe all matching lineitem rows
            uint32_t bucket = murmurhash32(partkey) % ht_num_buckets;
            uint32_t idx = ht_heads[bucket];

            while (idx != UINT32_MAX) {
                if (ht_entries[idx].key == partkey) {
                    int32_t lineitem_row = ht_entries[idx].rowid;
                    uint32_t pos = atomicAdd(result_count, 1);
                    out_extendedprice[pos] = filtered_extendedprice[lineitem_row];
                    out_discount[pos] = filtered_discount[lineitem_row];
                }
                idx = ht_entries[idx].next;
            }
        }
    }
}

inline Q19Result allocate_q19_result(size_t capacity, cudaStream_t stream = 0)
{
    Q19Result result;
    result.capacity = capacity;

    GPU_CHECK(cudaMalloc(&result.extendedprice, capacity * sizeof(double)));
    GPU_CHECK(cudaMalloc(&result.discount, capacity * sizeof(double)));
    GPU_CHECK(cudaMalloc(&result.count, sizeof(uint32_t)));
    GPU_CHECK(cudaMemsetAsync(result.count, 0, sizeof(uint32_t), stream));

    return result;
}

inline void free_q19_result(Q19Result& result)
{
    cudaFree(result.extendedprice);
    cudaFree(result.discount);
    cudaFree(result.count);
}

inline uint32_t run_q19(const PartColumns& part,
                        const LineitemColumns& lineitem,
                        Q19Result& result,
                        cudaStream_t stream = 0)
{
    // Hash table for filtered lineitem rows (build side)
    HashTable ht = allocate_hash_table(lineitem.num_rows, stream);

    // Intermediate storage for filtered lineitem data
    double* filtered_extendedprice;
    double* filtered_discount;
    uint32_t* filtered_count;
    GPU_CHECK(cudaMalloc(&filtered_extendedprice, lineitem.num_rows * sizeof(double)));
    GPU_CHECK(cudaMalloc(&filtered_discount, lineitem.num_rows * sizeof(double)));
    GPU_CHECK(cudaMalloc(&filtered_count, sizeof(uint32_t)));
    GPU_CHECK(cudaMemsetAsync(filtered_count, 0, sizeof(uint32_t), stream));

    GPU_CHECK(cudaMemsetAsync(result.count, 0, sizeof(uint32_t), stream));

    launchQuery(q19_kernel,
                stream,
                // Lineitem (build side)
                lineitem.l_partkey,
                lineitem.l_extendedprice,
                lineitem.l_discount,
                lineitem.l_quantity,
                lineitem.l_shipmode,
                lineitem.l_shipinstruct,
                lineitem.num_rows,
                // Part (probe side)
                part.p_partkey,
                part.p_brand,
                part.p_container,
                part.p_size,
                part.num_rows,
                // Hash table
                ht.entries,
                ht.heads,
                ht.counter,
                ht.num_buckets,
                ht.capacity,
                // Filtered storage
                filtered_extendedprice,
                filtered_discount,
                filtered_count,
                // Output
                result.extendedprice,
                result.discount,
                result.count);

    uint32_t count;
    GPU_CHECK(cudaMemcpyAsync(&count, result.count, sizeof(uint32_t), cudaMemcpyDeviceToHost, stream));
    GPU_CHECK(cudaStreamSynchronize(stream));

    // Cleanup
    GPU_CHECK(cudaFree(filtered_extendedprice));
    GPU_CHECK(cudaFree(filtered_discount));
    GPU_CHECK(cudaFree(filtered_count));
    free_hash_table(ht);

    return count;
}

} // namespace gpu_native
