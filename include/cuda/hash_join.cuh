#pragma once

#include "cuda/commands.hpp"
#include "cuda/hash_table.hpp"
#include "cuda/helper.hpp"
#include "data/type_traits.hpp"

#include <cuda_runtime.h>

namespace velodb::cuda {

__device__ __forceinline__ uint64_t murmurhash3_64(uint64_t key)
{
    key ^= key >> 33;
    key *= 0xff51afd7ed558ccd;
    key ^= key >> 33;
    key *= 0xc4ceb9fe1a85ec53;
    key ^= key >> 33;
    return key;
}

template <typename KeyT>
__device__ __forceinline__ uint64_t hash_key(const KeyT& key)
{
    return murmurhash3_64(static_cast<uint64_t>(key));
}

template <typename KeyT>
__device__ __forceinline__ void insertHashTable(HashTableEntry<KeyT>* entries,
                                                uint32_t* heads,
                                                uint32_t* counter,
                                                uint32_t num_buckets,
                                                uint32_t capacity,
                                                const KeyT& key,
                                                int64_t rowid)
{
    uint64_t hash = hash_key<KeyT>(key);
    uint32_t bucket_idx = hash % num_buckets;

    uint32_t entry_idx = atomicAdd(counter, 1);
    if (entry_idx >= capacity) {
        return; // Table full
    }

    HashTableEntry<KeyT>& entry = entries[entry_idx];
    entry.key = key;
    entry.rowid = rowid;
    uint32_t old_head = atomicExch(&heads[bucket_idx], entry_idx);
    entry.next = old_head;
}

template <typename KeyT>
__device__ __forceinline__ uint32_t
countHashTableMatches(HashTableEntry<KeyT>* entries, uint32_t* heads, uint32_t num_buckets, const KeyT& key)
{
    uint64_t hash = hash_key<KeyT>(key);
    uint32_t bucket_idx = hash % num_buckets;

    uint32_t count = 0;
    uint32_t current_idx = heads[bucket_idx];
    while (current_idx != HASH_TABLE_EMPTY) {
        HashTableEntry<KeyT>& entry = entries[current_idx];
        if (entry.key == key) {
            count++;
        }
        current_idx = entry.next;
    }
    return count;
}

template <typename KeyT>
__device__ __forceinline__ void probeHashTableWrite(HashTableEntry<KeyT>* entries,
                                                    uint32_t* heads,
                                                    uint32_t num_buckets,
                                                    const KeyT& key,
                                                    int64_t probe_rowid,
                                                    int64_t* out_left,
                                                    int64_t* out_right,
                                                    uint32_t* write_offset)
{
    uint64_t hash = hash_key<KeyT>(key);
    uint32_t bucket_idx = hash % num_buckets;

    uint32_t current_idx = heads[bucket_idx];
    while (current_idx != HASH_TABLE_EMPTY) {
        HashTableEntry<KeyT>& entry = entries[current_idx];
        if (entry.key == key) {
            uint32_t match_idx = atomicAdd(write_offset, 1);
            out_left[match_idx] = entry.rowid;
            out_right[match_idx] = probe_rowid;
        }
        current_idx = entry.next;
    }
}

// ============================================================================
// Hash Join Build Implementation
// ============================================================================

template <typename KeyT>
__device__ __forceinline__ void hashJoinBuildImpl(const KeyT* keys,
                                                  size_t n,
                                                  HashTableEntry<KeyT>* ht_entries,
                                                  uint32_t* ht_heads,
                                                  uint32_t* ht_counter,
                                                  uint32_t ht_capacity,
                                                  uint32_t ht_num_buckets,
                                                  cg::grid_group& grid)
{
    size_t tid = grid.thread_rank();
    size_t stride = grid.size();

    for (size_t i = tid; i < n; i += stride) {
        insertHashTable<KeyT>(ht_entries, ht_heads, ht_counter, ht_num_buckets, ht_capacity, keys[i], i);
    }
}

// ============================================================================
// Hash Join Count Implementation
// ============================================================================

template <typename KeyT>
__device__ __forceinline__ void hashJoinCountImpl(const KeyT* probe_keys,
                                                  size_t probe_n,
                                                  HashTableEntry<KeyT>* ht_entries,
                                                  uint32_t* ht_heads,
                                                  uint32_t ht_num_buckets,
                                                  size_t* out_count,
                                                  cg::grid_group& grid)
{
    size_t tid = grid.thread_rank();
    size_t stride = grid.size();

    for (size_t i = tid; i < probe_n; i += stride) {
        uint32_t matches = countHashTableMatches<KeyT>(ht_entries, ht_heads, ht_num_buckets, probe_keys[i]);
        if (matches > 0) {
            atomicAdd((unsigned long long*)out_count, matches);
        }
    }
}

// ============================================================================
// Hash Join Write Implementation
// ============================================================================

template <typename KeyT>
__device__ __forceinline__ void hashJoinWriteImpl(const KeyT* probe_keys,
                                                  const int64_t* probe_rowids,
                                                  size_t probe_n,
                                                  HashTableEntry<KeyT>* ht_entries,
                                                  uint32_t* ht_heads,
                                                  uint32_t ht_num_buckets,
                                                  int64_t* out_left,
                                                  int64_t* out_right,
                                                  uint32_t* write_offset,
                                                  cg::grid_group& grid)
{
    size_t tid = grid.thread_rank();
    size_t stride = grid.size();

    for (size_t i = tid; i < probe_n; i += stride) {
        probeHashTableWrite<KeyT>(ht_entries,
                                  ht_heads,
                                  ht_num_buckets,
                                  probe_keys[i],
                                  probe_rowids[i],
                                  out_left,
                                  out_right,
                                  write_offset);
    }
}

// ============================================================================
// Dispatch Functions
// ============================================================================

__device__ inline void executeHashJoinBuild(const CommandArgs::HashJoinBuildArgs& args, cg::grid_group& grid)
{
    switch (args.type_id) {
#define X(name, DT, VT)                                                                                                \
    case DataTypeId::name: {                                                                                           \
        hashJoinBuildImpl<DT>(static_cast<const DT*>(args.keys),                                                       \
                              args.n,                                                                                  \
                              static_cast<HashTableEntry<DT>*>(args.ht.entries),                                       \
                              args.ht.heads,                                                                           \
                              args.ht.counter,                                                                         \
                              args.ht.capacity,                                                                        \
                              args.ht.num_buckets,                                                                     \
                              grid);                                                                                   \
        break;                                                                                                         \
    }
        LIST_TYPES(X)
#undef X
    default:
        break;
    }
}

__device__ inline void executeHashJoinCount(const CommandArgs::HashJoinCountArgs& args, cg::grid_group& grid)
{
    switch (args.type_id) {
#define X(name, DT, VT)                                                                                                \
    case DataTypeId::name: {                                                                                           \
        hashJoinCountImpl<DT>(static_cast<const DT*>(args.probe_keys),                                                 \
                              args.probe_n,                                                                            \
                              static_cast<HashTableEntry<DT>*>(args.ht.entries),                                       \
                              args.ht.heads,                                                                           \
                              args.ht.num_buckets,                                                                     \
                              args.out_count,                                                                          \
                              grid);                                                                                   \
        break;                                                                                                         \
    }
        LIST_TYPES(X)
#undef X
    default:
        break;
    }
}

__device__ inline void executeHashJoinWrite(const CommandArgs::HashJoinWriteArgs& args, cg::grid_group& grid)
{
    switch (args.type_id) {
#define X(name, DT, VT)                                                                                                \
    case DataTypeId::name: {                                                                                           \
        hashJoinWriteImpl<DT>(static_cast<const DT*>(args.probe_keys),                                                 \
                              args.probe_rowids,                                                                       \
                              args.probe_n,                                                                            \
                              static_cast<HashTableEntry<DT>*>(args.ht.entries),                                       \
                              args.ht.heads,                                                                           \
                              args.ht.num_buckets,                                                                     \
                              args.out_left,                                                                           \
                              args.out_right,                                                                          \
                              args.write_offset,                                                                       \
                              grid);                                                                                   \
        break;                                                                                                         \
    }
        LIST_TYPES(X)
#undef X
    default:
        break;
    }
}

} // namespace velodb::cuda
