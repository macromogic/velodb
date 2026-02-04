#pragma once

#include "operator/abstract_operator.hpp"

#include <random>
#include <unordered_map>

namespace velodb {

// Forward declaration
class ObliviousTable;

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

    TableRecoveryResult obliviousRecoverSingle(const Table* source_table,
                                               ObliviousTable* oblivious_table,
                                               const std::vector<int64_t>& record_ids,
                                               const std::vector<size_t>& column_indices);

    void bitonicSortTuples(std::vector<SortableTuple>& tuples);

    void gpuBitonicSortTuples(std::vector<SortableTuple>& tuples);

    static void obliviousCompareSwap(SortableTuple& a, SortableTuple& b, bool ascending);

    Column obliviousGatherColumn(const Column& src_col,
                                 const std::vector<uint32_t>& sorted_positions,
                                 const std::vector<int64_t>& sorted_rowids,
                                 const std::vector<uint32_t>& restore_permutation,
                                 size_t n,
                                 std::uniform_int_distribution<uint32_t>& dist);

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
