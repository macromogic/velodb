#pragma once

#include <cstdint>

namespace velodb {

constexpr uint32_t HASH_TABLE_EMPTY = 0xFFFFFFFF;

template <typename T>
struct HashTableEntry {
    T key;
    int64_t rowid;
    uint32_t next; // HASH_TABLE_EMPTY if end of chain
};

template <typename T>
struct HashTable {
    HashTableEntry<T>* entries;
    uint32_t* heads;
    uint32_t* counter;
    uint32_t capacity;
    uint32_t num_buckets;
};

} // namespace velodb
