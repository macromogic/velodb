#pragma once

#include "common/copy_traits.hpp"
#include "data/data_location.hpp"
#include "data/ordinal_string.hpp"
#include "data/value_vector.hpp"

#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace velodb {

/**
 * @brief Memory-efficient builder for string columns.
 *
 * Instead of storing Value objects (~80 bytes each), this builder uses:
 * 1. A compact string pool (all strings concatenated)
 * 2. Offset array to locate each string
 * 3. Hash map for deduplication during insert
 *
 * Memory usage comparison for N strings of average length L:
 * - Value-based: N * 80 bytes (Value overhead)
 * - This builder: N * 4 bytes (offsets) + N * L bytes (unique strings)
 *
 * For TPC-H lineitem (300M rows, SF=50):
 * - Value-based: ~24 GB just for Value objects
 * - This builder: ~1.2 GB for offsets + string data
 */
class StringColumnBuilder : private NonCopyable {
public:
    explicit StringColumnBuilder(size_t estimated_rows = 0)
    {
        if (estimated_rows > 0) {
            ordinals_.reserve(estimated_rows);
            null_flags_.reserve(estimated_rows);
        }
    }

    /**
     * @brief Append a string value.
     * @param str The string to append (will be deduplicated)
     */
    void append(std::string_view str)
    {
        null_flags_.push_back(false);

        // Check if string already exists in dictionary
        // Use string key directly to avoid dangling string_view when vector reallocates
        std::string str_key(str);
        auto it = string_to_id_.find(str_key);
        if (it != string_to_id_.end()) {
            ordinals_.push_back(it->second);
            return;
        }

        // New unique string - add to pool
        size_t id = unique_strings_.size();
        unique_strings_.push_back(std::move(str_key));

        // Add to lookup map
        string_to_id_[unique_strings_.back()] = id;
        ordinals_.push_back(id);
    }

    /**
     * @brief Append a null value.
     */
    void appendNull()
    {
        null_flags_.push_back(true);
        ordinals_.push_back(0); // Placeholder, won't be used
    }

    /**
     * @brief Get the number of rows.
     */
    size_t size() const { return ordinals_.size(); }

    /**
     * @brief Get the number of unique strings.
     */
    size_t uniqueCount() const { return unique_strings_.size(); }

    /**
     * @brief Get approximate memory usage in bytes.
     */
    size_t memoryUsage() const
    {
        size_t usage = ordinals_.capacity() * sizeof(size_t);
        usage += null_flags_.capacity() * sizeof(bool);
        usage += string_to_id_.bucket_count() * sizeof(void*) * 2; // Rough estimate
        for (const auto& s : unique_strings_) {
            usage += s.capacity() + sizeof(std::string);
        }
        return usage;
    }

    /**
     * @brief Build a ValueVector<OrdinalString> from the collected data.
     *
     * This method sorts the dictionary and remaps ordinals to maintain
     * the sorted order required by OrdinalString comparison operations.
     *
     * @param location Where to allocate the result
     * @return A complete ValueVector<OrdinalString>
     */
    ValueVector<OrdinalString> build(DataLocation location = DataLocation::HOST)
    {
        size_t n = ordinals_.size();
        ValueVector<OrdinalString> vec(n, location);

        if (unique_strings_.empty()) {
            // No strings - just set nulls
            for (size_t i = 0; i < n; ++i) {
                if (null_flags_[i]) {
                    vec.setNull(i);
                }
            }
            vec.setSize(n);
            return vec;
        }

        // Create sorted index for unique_strings_
        std::vector<size_t> sort_indices(unique_strings_.size());
        for (size_t i = 0; i < sort_indices.size(); ++i) {
            sort_indices[i] = i;
        }
        std::sort(sort_indices.begin(), sort_indices.end(), [this](size_t a, size_t b) {
            return unique_strings_[a] < unique_strings_[b];
        });

        // Build reverse mapping: old_id -> new_ordinal (sorted position)
        std::vector<size_t> id_to_ordinal(unique_strings_.size());
        for (size_t new_ord = 0; new_ord < sort_indices.size(); ++new_ord) {
            id_to_ordinal[sort_indices[new_ord]] = new_ord;
        }

        // Populate ordered_strings_ in sorted order
        vec.setDictionary(unique_strings_.size());
        for (size_t i = 0; i < sort_indices.size(); ++i) {
            vec.setDictionaryEntry(i, std::move(unique_strings_[sort_indices[i]]));
        }

        // Remap ordinals and set null mask
        for (size_t i = 0; i < n; ++i) {
            if (null_flags_[i]) {
                vec.setNull(i);
            } else {
                vec.setOrdinal(i, id_to_ordinal[ordinals_[i]]);
            }
        }

        vec.setSize(n);

        // Clear our data to free memory
        ordinals_.clear();
        ordinals_.shrink_to_fit();
        null_flags_.clear();
        null_flags_.shrink_to_fit();
        unique_strings_.clear();
        unique_strings_.shrink_to_fit();
        string_to_id_.clear();

        return vec;
    }

    /**
     * @brief Reserve capacity for the expected number of rows.
     */
    void reserve(size_t row_count)
    {
        ordinals_.reserve(row_count);
        null_flags_.reserve(row_count);
    }

private:
    // Temporary storage during building
    std::vector<size_t> ordinals_; // Per-row ordinal (index into unique_strings_)
    std::vector<bool> null_flags_; // Per-row null flag
    std::vector<std::string> unique_strings_; // Deduplicated strings
    std::unordered_map<std::string, size_t> string_to_id_; // For O(1) dedup lookup (string key for stability)
};

} // namespace velodb
