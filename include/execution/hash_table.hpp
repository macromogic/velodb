#pragma once

#include "catalog/schema.hpp"
#include "types/value.hpp"
#include <memory>
#include <vector>

namespace velodb {

// Forward declarations
class Tuple;

// Hash table entry for join operations
struct HashTableEntry {
    std::vector<Value> key_values_;
    std::vector<Tuple> tuples_; // All tuples with the same key

    explicit HashTableEntry(std::vector<Value> keys)
        : key_values_(std::move(keys))
    {
    }
};

// Hash table interface for join operations
class HashTable {
public:
    HashTable(const Schema& key_schema, const Schema& value_schema);
    ~HashTable() = default;

    // Delete copy constructor and assignment
    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;

    // Insert a tuple with the given key values
    void insert(const std::vector<Value>& key_values, const Tuple& tuple);
    void insert(const std::vector<Value>& key_values, Tuple&& tuple);

    // Lookup tuples by key values
    [[nodiscard]] const std::vector<Tuple>* lookup(const std::vector<Value>& key_values) const;

    // Get all entries (for iteration)
    [[nodiscard]] const std::vector<std::unique_ptr<HashTableEntry>>& getEntries() const { return entries_; }

    // Statistics
    [[nodiscard]] size_t getSize() const { return entries_.size(); }
    [[nodiscard]] bool isEmpty() const { return entries_.empty(); }

    // Clear all entries
    void clear();

private:
    // TODO: Implement efficient hash table with proper collision handling
    // For now, use simple vector-based implementation
    std::vector<std::unique_ptr<HashTableEntry>> entries_;
    const Schema& key_schema_;
    const Schema& value_schema_;

    // Hash function for key values
    [[nodiscard]] static size_t hashKey(const std::vector<Value>& key_values);

    // Equality comparison for key values
    [[nodiscard]] static bool keysEqual(const std::vector<Value>& key1, const std::vector<Value>& key2);
};

} // namespace velodb
