#pragma once

#include "types.hpp"

#include <cooperative_groups.h>
#include <cuda_runtime.h>

namespace cg = cooperative_groups;

namespace gpu_native {

// ============================================================================
// MurmurHash3 - 64-bit finalizer
// ============================================================================

__device__ __forceinline__ uint64_t murmurhash64(int64_t key)
{
    uint64_t k = static_cast<uint64_t>(key);
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return k;
}

__device__ __forceinline__ uint32_t murmurhash32(int32_t key)
{
    uint32_t k = static_cast<uint32_t>(key);
    k ^= k >> 16;
    k *= 0x85ebca6b;
    k ^= k >> 13;
    k *= 0xc2b2ae35;
    k ^= k >> 16;
    return k;
}

// ============================================================================
// Hash Table Operations
// ============================================================================

__device__ __forceinline__ void hash_table_insert(HashEntry* entries,
                                                  uint32_t* heads,
                                                  uint32_t* counter,
                                                  uint32_t num_buckets,
                                                  uint32_t capacity,
                                                  int32_t key,
                                                  int32_t rowid)
{
    uint32_t slot = atomicAdd(counter, 1);
    if (slot >= capacity)
        return; // overflow protection

    entries[slot].key = key;
    entries[slot].rowid = rowid;

    uint32_t bucket = murmurhash32(key) % num_buckets;
    entries[slot].next = atomicExch(&heads[bucket], slot);
}

__device__ __forceinline__ void hash_table_insert(HashTable& ht, int32_t key, int32_t rowid)
{
    hash_table_insert(ht.entries, ht.heads, ht.counter, ht.num_buckets, ht.capacity, key, rowid);
}

// Probe and return first matching rowid, or -1 if not found
__device__ __forceinline__ int32_t hash_table_probe(const HashEntry* entries,
                                                    const uint32_t* heads,
                                                    uint32_t num_buckets,
                                                    int32_t key)
{
    uint32_t bucket = murmurhash32(key) % num_buckets;
    uint32_t curr = heads[bucket];

    while (curr != HASH_EMPTY) {
        if (entries[curr].key == key) {
            return entries[curr].rowid;
        }
        curr = entries[curr].next;
    }
    return -1;
}

__device__ __forceinline__ int32_t hash_table_probe(const HashTable& ht, int32_t key)
{
    return hash_table_probe(ht.entries, ht.heads, ht.num_buckets, key);
}

// Check existence only
__device__ __forceinline__ bool hash_table_exists(const HashEntry* entries,
                                                  const uint32_t* heads,
                                                  uint32_t num_buckets,
                                                  int32_t key)
{
    uint32_t bucket = murmurhash32(key) % num_buckets;
    uint32_t curr = heads[bucket];

    while (curr != HASH_EMPTY) {
        if (entries[curr].key == key) {
            return true;
        }
        curr = entries[curr].next;
    }
    return false;
}

__device__ __forceinline__ bool hash_table_exists(const HashTable& ht, int32_t key)
{
    return hash_table_exists(ht.entries, ht.heads, ht.num_buckets, key);
}

// Count matches (for 1:N joins)
__device__ __forceinline__ uint32_t hash_table_count_matches(const HashEntry* entries,
                                                             const uint32_t* heads,
                                                             uint32_t num_buckets,
                                                             int32_t key)
{
    uint32_t bucket = murmurhash32(key) % num_buckets;
    uint32_t curr = heads[bucket];
    uint32_t count = 0;

    while (curr != HASH_EMPTY) {
        if (entries[curr].key == key) {
            count++;
        }
        curr = entries[curr].next;
    }
    return count;
}

// Iterate all matches and call callback
template <typename Callback>
__device__ __forceinline__ void hash_table_for_each_match(const HashEntry* entries,
                                                          const uint32_t* heads,
                                                          uint32_t num_buckets,
                                                          int32_t key,
                                                          Callback&& callback)
{
    uint32_t bucket = murmurhash32(key) % num_buckets;
    uint32_t curr = heads[bucket];

    while (curr != HASH_EMPTY) {
        if (entries[curr].key == key) {
            callback(entries[curr].rowid);
        }
        curr = entries[curr].next;
    }
}

// ============================================================================
// Hash Table Initialization
// ============================================================================

__global__ void init_hash_table_heads(uint32_t* heads, uint32_t num_buckets)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < num_buckets) {
        heads[idx] = HASH_EMPTY;
    }
}

inline void init_hash_table(HashTable& ht, cudaStream_t stream = 0)
{
    int block_size = 256;
    int num_blocks = (ht.num_buckets + block_size - 1) / block_size;
    init_hash_table_heads<<<num_blocks, block_size, 0, stream>>>(ht.heads, ht.num_buckets);
    cudaMemsetAsync(ht.counter, 0, sizeof(uint32_t), stream);
}

// ============================================================================
// Hash Table Allocation Helper
// ============================================================================

inline HashTable allocate_hash_table(size_t expected_rows, cudaStream_t stream = 0)
{
    HashTable ht;
    ht.capacity = static_cast<uint32_t>(expected_rows * 1.5); // 1.5x for safety
    ht.num_buckets = static_cast<uint32_t>(expected_rows * 2); // 2x for load factor ~0.5

    cudaMalloc(&ht.entries, ht.capacity * sizeof(HashEntry));
    cudaMalloc(&ht.heads, ht.num_buckets * sizeof(uint32_t));
    cudaMalloc(&ht.counter, sizeof(uint32_t));

    init_hash_table(ht, stream);
    return ht;
}

inline void free_hash_table(HashTable& ht)
{
    cudaFree(ht.entries);
    cudaFree(ht.heads);
    cudaFree(ht.counter);
    ht.entries = nullptr;
    ht.heads = nullptr;
    ht.counter = nullptr;
}

} // namespace gpu_native
