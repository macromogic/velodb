#pragma once

#include "operator/abstract_operator.hpp"

#include <random>
#include <unordered_map>

namespace velodb {

// Forward declaration
class ObliviousTable;

/**
 * @brief Oblivious materialization operator with security guarantees.
 *
 * Implements the oblivious recovery algorithm to prevent access pattern leakage:
 * - Uses bitonic sort for oblivious ordering (data-independent access pattern)
 * - O(1) space deduplication with dummy accesses to hide repeated queries
 * - Maintains pairing relationships across multiple tables via pairing_idx
 *
 * Algorithm overview:
 * 1. Build recovery requests with pairing indices for each table
 * 2. Per-table oblivious recovery:
 *    a. Create sortable tuples (record_id, position, pairing_idx)
 *    b. Bitonic sort by record_id (makes duplicates adjacent)
 *    c. O(1) cache scan: real access for new IDs, dummy access for duplicates
 *    d. Restore original order by pairing_idx
 * 3. Merge results - position i from each table corresponds to JOIN pair i
 */
class MaterializationOperator : public UnaryOperator {
    struct ColumnMapping {
        size_t rowid_input_index; // Index in input batch containing rowids
        const Table* source_table; // Source table pointer
        ObliviousTable* oblivious_table; // Oblivious wrapper (nullptr if not enabled)
        size_t source_col_index; // Column index in source table
    };

    // Sortable tuple for oblivious recovery algorithm
    struct SortableTuple {
        int64_t record_id; // Row identifier (sort key)
        uint32_t position; // Physical position in (shuffled) table
        uint32_t pairing_idx; // Original index for restoring order after sort
    };

    // Column-based recovery result for batch efficiency
    // Instead of vector<vector<Value>> (rows), we return vector<Column> (columns)
    struct TableRecoveryResult {
        std::vector<Column> columns; // columns[c][i] = value at row i, column c
    };

public:
    MaterializationOperator(ExecutionContext& context, Schema output_schema, std::unique_ptr<AbstractOperator> child);
    ~MaterializationOperator() override = default;

    Result<RowBatch> next() override;

private:
    // ========================================================================
    // Oblivious Recovery Algorithm
    // ========================================================================

    /**
     * @brief Recover data for a single table using oblivious algorithm
     *
     * @param source_table      The source table to recover from
     * @param oblivious_table   Oblivious wrapper (for position mapping), may be nullptr
     * @param record_ids        Row IDs to recover
     * @param column_indices    Which columns to fetch
     * @return Recovered rows ordered by original pairing index
     */
    TableRecoveryResult obliviousRecoverSingle(const Table* source_table,
                                               ObliviousTable* oblivious_table,
                                               const std::vector<int64_t>& record_ids,
                                               const std::vector<size_t>& column_indices);

    /**
     * @brief Host-side bitonic sort (oblivious - fixed access pattern)
     *
     * Sorts tuples by record_id so duplicates become adjacent.
     * Access pattern is independent of data values.
     *
     * @note For large arrays, this is slow. Prefer gpuBitonicSortTuples for n > threshold.
     */
    void bitonicSortTuples(std::vector<SortableTuple>& tuples);

    /**
     * @brief GPU-accelerated bitonic sort using persistent kernel
     *
     * Uploads record_ids and pairing_idx to GPU, sorts by record_id,
     * downloads results. Much faster than CPU for large n (> 10000).
     *
     * @param tuples  Tuples to sort. Will be sorted in-place by record_id.
     */
    void gpuBitonicSortTuples(std::vector<SortableTuple>& tuples);

    /**
     * @brief Oblivious compare-and-swap for bitonic sort
     */
    static void obliviousCompareSwap(SortableTuple& a, SortableTuple& b, bool ascending);

    /**
     * @brief Type dispatcher for oblivious gather
     */
    Column obliviousGatherColumn(const Column& src_col,
                                 const std::vector<uint32_t>& sorted_positions,
                                 const std::vector<int64_t>& sorted_rowids,
                                 const std::vector<uint32_t>& restore_permutation,
                                 size_t n,
                                 std::uniform_int_distribution<uint32_t>& dist);

    /**
     * @brief Type-dispatched direct gather (writes directly to Column buffer)
     *
     * Directly copies underlying data arrays with O(1) cache deduplication.
     * For VARCHAR/CHAR, copies ordinal IDs (shares dictionary).
     */
    template <typename DType>
    Column obliviousGatherTyped(const Column& src_col,
                                const std::vector<uint32_t>& sorted_positions,
                                const std::vector<int64_t>& sorted_rowids,
                                const std::vector<uint32_t>& restore_permutation,
                                size_t n,
                                [[maybe_unused]] size_t table_rows,
                                std::uniform_int_distribution<uint32_t>& dist);

    // ========================================================================
    // Member Variables
    // ========================================================================

    std::vector<ColumnMapping> col_map_;

    // Group columns by source table for efficient batch recovery
    std::unordered_map<const Table*, std::vector<size_t>> table_to_columns_;
    std::unordered_map<const Table*, size_t> table_to_rowid_idx_;
    std::unordered_map<const Table*, ObliviousTable*> table_to_oblivious_;

    // RNG for dummy access positions
    std::mt19937_64 rng_;
};

} // namespace velodb
