#include "operator/materialization_operator.hpp"

#include "catalog/catalog.hpp"
#include "catalog/row_batch.hpp"
#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include "common/string_utils.hpp"
#include "cuda/commands.hpp"
#include "cuda/stream.hpp"
#include "data/oblivious_table.hpp"
#include "data/oblivious_table_manager.hpp"

#include <algorithm>
#include <random>

#include <cuda_runtime.h>

namespace velodb {

// Threshold above which we use GPU sorting (GPU has overhead for small n)
constexpr size_t GPU_SORT_THRESHOLD = 100000;

// ============================================================================
// Constructor
// ============================================================================

MaterializationOperator::MaterializationOperator(ExecutionContext& context,
                                                 Schema output_schema,
                                                 std::unique_ptr<AbstractOperator> child)
    : UnaryOperator(context, std::move(output_schema), std::move(child))
    , rng_(std::random_device {}())
{
    // Identify available tables and their RowID column index from the input child operator
    // The child operator (Projection of RowIDs) must output columns named "TableName.$_rowid"
    std::unordered_map<std::string, size_t> table_rowid_indices;
    std::unordered_map<std::string, const Table*> tables;

    const auto& in_schema = child_->getOutputSchema();
    for (size_t i = 0; i < in_schema.getColumnCount(); ++i) {
        auto full_name = in_schema.getColumnInfo(i).getName();
        auto [table_name_view, col_name_view] = splitName(full_name);

        // We expect inputs to be rowids.
        if (col_name_view == "$_rowid") {
            std::string table_name(table_name_view);
            table_rowid_indices[table_name] = i;
            if (tables.find(table_name) == tables.end()) {
                auto table_opt = context_.getCatalog().getTable(table_name);
                VELODB_ASSERT_MSG(table_opt.has_value(), "Source table must exist in catalog");
                tables[table_name] = &table_opt.value().get();

                // Track oblivious table if available
                if (context_.getCatalog().hasObliviousManager()) {
                    auto& oblivious_mgr = context_.getCatalog().getObliviousManager();
                    if (oblivious_mgr.hasTable(table_name)) {
                        table_to_oblivious_[tables[table_name]] = &oblivious_mgr.getTable(table_name);
                    }
                }
            }
        }
    }

    const auto& out_schema = getOutputSchema();
    // Build column maps and group by table
    for (size_t i = 0; i < out_schema.getColumnCount(); ++i) {
        const auto& col_info = out_schema.getColumnInfo(i);
        auto full_name = col_info.getName();
        auto [table_name_view, col_name_view] = splitName(full_name);
        std::string table_name(table_name_view);
        std::string col_name(col_name_view);

        const Table* source_table = nullptr;
        size_t rowid_idx = 0;

        if (table_name.empty()) {
            for (const auto& [t_name, t_ptr] : tables) {
                bool found = false;
                for (size_t j = 0; j < t_ptr->getColumnCount(); ++j) {
                    if (t_ptr->getColumnName(j) == col_name) {
                        found = true;
                        break;
                    }
                }

                if (found) {
                    if (source_table != nullptr) {
                        VELODB_THROW(DatabaseError, "Ambiguous column name in materialization: " + col_name);
                    }
                    source_table = t_ptr;
                    rowid_idx = table_rowid_indices[t_name];
                }
            }
            if (source_table == nullptr) {
                VELODB_THROW(DatabaseError, "Output schema column does not match any source table: " + col_name);
            }
        } else {
            auto it = tables.find(table_name);
            if (it == tables.end()) {
                VELODB_THROW(DatabaseError, "Table not available in join results: " + table_name);
            }
            source_table = it->second;
            rowid_idx = table_rowid_indices[table_name];
        }

        // Find column index in source table
        size_t source_col_idx = source_table->getColumnIndex(col_name);

        // Get oblivious table wrapper if available
        ObliviousTable* obl_table = nullptr;
        auto obl_it = table_to_oblivious_.find(source_table);
        if (obl_it != table_to_oblivious_.end()) {
            obl_table = obl_it->second;
        }

        col_map_.push_back({ rowid_idx, source_table, obl_table, source_col_idx });

        // Group columns by table for batch recovery
        table_to_columns_[source_table].push_back(source_col_idx);
        table_to_rowid_idx_[source_table] = rowid_idx;
    }
}

// ============================================================================
// Oblivious Bitonic Sort (Host-side, Data-independent access pattern)
// ============================================================================

void MaterializationOperator::obliviousCompareSwap(SortableTuple& a, SortableTuple& b, bool ascending)
{
    // Data-oblivious compare-and-swap: always executes the same operations
    // regardless of data values (no early exit based on comparison result)
    bool should_swap = ascending ? (a.record_id > b.record_id) : (a.record_id < b.record_id);
    if (should_swap) {
        std::swap(a, b);
    }
}

void MaterializationOperator::bitonicSortTuples(std::vector<SortableTuple>& tuples)
{
    size_t n = tuples.size();
    if (n <= 1)
        return;

    // Pad to next power of 2 (required for bitonic sort)
    size_t padded_n = nextPow2(n);
    size_t original_n = n;

    // Add sentinel values for padding (will sort to end)
    while (tuples.size() < padded_n) {
        tuples.push_back({ INT64_MAX, 0, UINT32_MAX });
    }

    // Bitonic sort: fixed comparison sequence independent of data
    // Outer loop: k controls the size of bitonic sequences
    for (size_t k = 2; k <= padded_n; k <<= 1) {
        // Inner loop: j controls the distance of compared elements
        for (size_t j = k >> 1; j > 0; j >>= 1) {
            // Compare-swap all pairs at distance j
            for (size_t i = 0; i < padded_n; ++i) {
                size_t ixj = i ^ j;
                if (i < ixj) {
                    // Ascending if in first half of bitonic sequence
                    bool ascending = ((i & k) == 0);
                    obliviousCompareSwap(tuples[i], tuples[ixj], ascending);
                }
            }
        }
    }

    // Remove padding (sentinels sorted to end)
    tuples.resize(original_n);
}

// ============================================================================
// GPU-accelerated Bitonic Sort using Persistent Kernel
// ============================================================================

void MaterializationOperator::gpuBitonicSortTuples(std::vector<SortableTuple>& tuples)
{
    PROFILE_SCOPE("GPU Bitonic Sort (Materialization)");

    size_t n = tuples.size();
    if (n <= 1)
        return;

    size_t padded_n = nextPow2(n);

    auto& task_manager = context_.getTaskManager();
    auto stream_handle = StreamPool::getInstance().acquire().value();

    // Allocate GPU buffers for record_ids and indices (pairing_idx)
    int64_t* d_record_ids;
    int64_t* d_indices;
    CHECKED_CALL_THROW(cudaMallocAsync(&d_record_ids, padded_n * sizeof(int64_t), stream_handle->get()));
    CHECKED_CALL_THROW(cudaMallocAsync(&d_indices, padded_n * sizeof(int64_t), stream_handle->get()));

    // Prepare host data: separate record_ids
    std::vector<int64_t> h_record_ids(padded_n);
    for (size_t i = 0; i < n; ++i) {
        h_record_ids[i] = tuples[i].record_id;
    }
    // Fill padding with max values (will sort to end)
    for (size_t i = n; i < padded_n; ++i) {
        h_record_ids[i] = INT64_MAX;
    }

    // Upload record_ids
    CHECKED_CALL_THROW(cudaMemcpyAsync(d_record_ids,
                                       h_record_ids.data(),
                                       padded_n * sizeof(int64_t),
                                       cudaMemcpyHostToDevice,
                                       stream_handle->get()));
    stream_handle->synchronize();

    // OP_SORT will initialize indices to [0, 1, 2, ...] and sort by record_ids
    Command sort_cmd;
    sort_cmd.opcode = OpCode::OP_SORT;
    sort_cmd.args = { .sort = {
                          .sort_cols = { d_record_ids },
                          .indices = d_indices,
                          .n_sort_columns = 1,
                          .n_rows = n,
                          .n_padded_rows = padded_n,
                          .col_types = { DataTypeId::BIGINT },
                          .ascending_flags = { true },
                      } };
    task_manager.waitCommand(task_manager.submitCommand(sort_cmd));

    // Download sorted indices (these are original positions: 0, 1, 2, ...)
    // After sort, d_indices[i] tells us which original tuple goes to position i
    std::vector<int64_t> h_sorted_indices(n);
    CHECKED_CALL_THROW(cudaMemcpyAsync(h_sorted_indices.data(),
                                       d_indices,
                                       n * sizeof(int64_t),
                                       cudaMemcpyDeviceToHost,
                                       stream_handle->get()));

    // Also download sorted record_ids (we need them for the gather phase)
    CHECKED_CALL_THROW(cudaMemcpyAsync(h_record_ids.data(),
                                       d_record_ids,
                                       n * sizeof(int64_t),
                                       cudaMemcpyDeviceToHost,
                                       stream_handle->get()));
    stream_handle->synchronize();

    // Free GPU memory
    CHECKED_CALL_THROW(cudaFreeAsync(d_record_ids, stream_handle->get()));
    CHECKED_CALL_THROW(cudaFreeAsync(d_indices, stream_handle->get()));
    stream_handle->synchronize();

    // Rebuild tuples in sorted order using the indices
    // h_sorted_indices[i] = original index that should be at position i
    std::vector<SortableTuple> original_tuples = std::move(tuples);
    tuples.resize(n);
    for (size_t i = 0; i < n; ++i) {
        size_t orig_idx = static_cast<size_t>(h_sorted_indices[i]);
        tuples[i].record_id = h_record_ids[i]; // Use sorted record_id
        tuples[i].position = original_tuples[orig_idx].position;
        tuples[i].pairing_idx = original_tuples[orig_idx].pairing_idx;
    }
}

// ============================================================================
// Type-dispatched column gather (calls templated obliviousGatherTyped)
// ============================================================================

Column MaterializationOperator::obliviousGatherColumn(const Column& src_col,
                                                      const std::vector<uint32_t>& sorted_positions,
                                                      const std::vector<int64_t>& sorted_rowids,
                                                      const std::vector<uint32_t>& restore_permutation,
                                                      size_t n,
                                                      std::uniform_int_distribution<uint32_t>& dist)
{
    DataTypeId type_id = src_col.getType().getTypeId();
    size_t table_rows = src_col.size();

    switch (type_id) {
    case DataTypeId::BOOLEAN:
        return obliviousGatherTyped<bool>(src_col,
                                          sorted_positions,
                                          sorted_rowids,
                                          restore_permutation,
                                          n,
                                          table_rows,
                                          dist);
    case DataTypeId::TINYINT:
        return obliviousGatherTyped<int8_t>(src_col,
                                            sorted_positions,
                                            sorted_rowids,
                                            restore_permutation,
                                            n,
                                            table_rows,
                                            dist);
    case DataTypeId::SMALLINT:
        return obliviousGatherTyped<int16_t>(src_col,
                                             sorted_positions,
                                             sorted_rowids,
                                             restore_permutation,
                                             n,
                                             table_rows,
                                             dist);
    case DataTypeId::INTEGER:
        return obliviousGatherTyped<int32_t>(src_col,
                                             sorted_positions,
                                             sorted_rowids,
                                             restore_permutation,
                                             n,
                                             table_rows,
                                             dist);
    case DataTypeId::BIGINT:
        return obliviousGatherTyped<int64_t>(src_col,
                                             sorted_positions,
                                             sorted_rowids,
                                             restore_permutation,
                                             n,
                                             table_rows,
                                             dist);
    case DataTypeId::FLOAT:
        return obliviousGatherTyped<float>(src_col,
                                           sorted_positions,
                                           sorted_rowids,
                                           restore_permutation,
                                           n,
                                           table_rows,
                                           dist);
    case DataTypeId::DOUBLE:
        return obliviousGatherTyped<double>(src_col,
                                            sorted_positions,
                                            sorted_rowids,
                                            restore_permutation,
                                            n,
                                            table_rows,
                                            dist);
    case DataTypeId::DATE:
        return obliviousGatherTyped<uint32_t>(src_col,
                                              sorted_positions,
                                              sorted_rowids,
                                              restore_permutation,
                                              n,
                                              table_rows,
                                              dist);
    case DataTypeId::VARCHAR:
    case DataTypeId::CHAR:
        // OrdinalString stores ordinal IDs as size_t
        return obliviousGatherTyped<size_t>(src_col,
                                            sorted_positions,
                                            sorted_rowids,
                                            restore_permutation,
                                            n,
                                            table_rows,
                                            dist);
    default:
        VELODB_THROW(ExecutionError, "Unsupported column type in oblivious materialization");
    }
}

// ============================================================================
// Single Table Oblivious Recovery (Direct ValueVector Access - High Performance)
// ============================================================================

MaterializationOperator::TableRecoveryResult MaterializationOperator::obliviousRecoverSingle(
    const Table* source_table,
    ObliviousTable* oblivious_table,
    const std::vector<int64_t>& record_ids,
    const std::vector<size_t>& column_indices)
{
    TableRecoveryResult result;
    size_t n = record_ids.size();
    if (n == 0)
        return result;

    size_t table_rows = source_table->getRowCount();
    size_t num_cols = column_indices.size();

    // ========================================================================
    // Step 1: Create sortable tuples with pairing indices
    // ========================================================================
    std::vector<SortableTuple> tuples(n);
    for (size_t i = 0; i < n; ++i) {
        int64_t rid = record_ids[i];
        tuples[i].record_id = rid;

        uint32_t logical_rowid = static_cast<uint32_t>(rid % table_rows);
        if (oblivious_table) {
            tuples[i].position = oblivious_table->getPosition(logical_rowid);
        } else {
            tuples[i].position = logical_rowid;
        }
        tuples[i].pairing_idx = static_cast<uint32_t>(i);
    }

    // ========================================================================
    // Step 2: Bitonic sort by record_id (oblivious - fixed access pattern)
    // Use GPU for large arrays (much faster than CPU O(n log² n))
    // ========================================================================
    if (n >= GPU_SORT_THRESHOLD) {
        gpuBitonicSortTuples(tuples);
    } else {
        bitonicSortTuples(tuples);
    }

    // ========================================================================
    // Step 3: Build sorted position array for batch gather
    // ========================================================================
    std::vector<uint32_t> sorted_positions(n);
    std::vector<int64_t> sorted_rowids(n);
    for (size_t i = 0; i < n; ++i) {
        sorted_positions[i] = tuples[i].position;
        sorted_rowids[i] = tuples[i].record_id;
    }

    // Build restore permutation
    std::vector<uint32_t> restore_permutation(n);
    for (size_t i = 0; i < n; ++i) {
        restore_permutation[tuples[i].pairing_idx] = static_cast<uint32_t>(i);
    }

    // ========================================================================
    // Step 4: Direct batch gather with O(1) cache (per column, type-dispatched)
    // This bypasses Value objects entirely for maximum performance
    // ========================================================================
    std::uniform_int_distribution<uint32_t> dist(0, static_cast<uint32_t>(table_rows - 1));

    result.columns.reserve(num_cols);

    for (size_t col_idx : column_indices) {
        const Column& src_col = source_table->getColumn(col_idx);
        [[maybe_unused]] DataTypeId type_id = src_col.getType().getTypeId();

        // Direct memory-level gather based on type
        Column gathered_col
            = obliviousGatherColumn(src_col, sorted_positions, sorted_rowids, restore_permutation, n, dist);

        result.columns.push_back(std::move(gathered_col));
    }

    return result;
}

// ============================================================================
// Templated direct gather - writes directly to Column's raw buffer
// ============================================================================

template <typename DType>
Column MaterializationOperator::obliviousGatherTyped(const Column& src_col,
                                                     const std::vector<uint32_t>& sorted_positions,
                                                     const std::vector<int64_t>& sorted_rowids,
                                                     const std::vector<uint32_t>& restore_permutation,
                                                     size_t n,
                                                     [[maybe_unused]] size_t table_rows,
                                                     std::uniform_int_distribution<uint32_t>& dist)
{
    // Get raw source data
    const DType* src_data = static_cast<const DType*>(src_col.rawData());
    const BitVector::Element* src_null_data = src_col.rawBitmapData();

    // Create output column with sufficient capacity
    // Using HOST_PAGEABLE to avoid exhausting pinned memory pool
    Column out_col(src_col.getType().cloneUnique(), n, DataLocation::HOST_PAGEABLE);
    DType* out_data = static_cast<DType*>(out_col.rawData());
    BitVector::Element* out_null_data = out_col.rawBitmapData();

    // Temporary buffer for sorted order (we need to restore order later)
    std::vector<DType> sorted_data(n);
    std::vector<bool> sorted_nulls(n, false);

    // O(1) cache for deduplication
    int64_t cached_rowid = INT64_MIN;
    DType cached_value {};
    bool cached_is_null = false;

    // Gather in sorted order with dummy access
    for (size_t i = 0; i < n; ++i) {
        int64_t rowid = sorted_rowids[i];
        uint32_t pos = sorted_positions[i];

        if (rowid == cached_rowid) {
            // DUPLICATE: dummy access + use cache
            uint32_t dummy_pos = dist(rng_);
            volatile DType dummy = src_data[dummy_pos];
            (void)dummy;

            sorted_data[i] = cached_value;
            sorted_nulls[i] = cached_is_null;
        } else {
            // NEW RECORD: real access + update cache
            sorted_data[i] = src_data[pos];
            cached_value = sorted_data[i];
            cached_rowid = rowid;

            // Check null bitmap
            if (src_null_data) {
                size_t word_idx = pos / (sizeof(BitVector::Element) * 8);
                size_t bit_idx = pos % (sizeof(BitVector::Element) * 8);
                cached_is_null = (src_null_data[word_idx] >> bit_idx) & 1;
                sorted_nulls[i] = cached_is_null;
            }
        }
    }

    // Restore original order - write directly to output column buffer
    for (size_t i = 0; i < n; ++i) {
        uint32_t src_idx = restore_permutation[i];
        out_data[i] = sorted_data[src_idx];

        // Set null bit if needed
        if (sorted_nulls[src_idx]) {
            size_t word_idx = i / (sizeof(BitVector::Element) * 8);
            size_t bit_idx = i % (sizeof(BitVector::Element) * 8);
            out_null_data[word_idx] |= (BitVector::Element(1) << bit_idx);
        }
    }

    // Note: size will be set via setNumRowsForBatch in next()
    return out_col;
}

// ============================================================================
// Main Materialization Entry Point
// ============================================================================

Result<RowBatch> MaterializationOperator::next()
{
    // Get batch from upstream
    auto result = child_->next();
    if (!result) {
        return result;
    }

    auto join_batch = std::move(result.value());
    if (join_batch.getRowCount() == 0) {
        return Result<RowBatch>::success(RowBatch()); // End of stream
    }

    PROFILE_SCOPE("Materialization");

    // Transfer to host for oblivious processing
    join_batch.to(DataLocation::HOST_PAGEABLE);

    size_t num_rows = join_batch.getRowCount();

    // ========================================================================
    // Phase 1: Collect unique tables and perform oblivious recovery per table
    // ========================================================================
    std::unordered_map<const Table*, TableRecoveryResult> table_results;

    for (const auto& [table, col_indices] : table_to_columns_) {
        size_t rowid_idx = table_to_rowid_idx_[table];
        const Column& rowid_col = join_batch.getColumn(rowid_idx);

        // Extract record IDs from the batch
        std::vector<int64_t> record_ids(num_rows);
        for (size_t i = 0; i < num_rows; ++i) {
            record_ids[i] = rowid_col.get(i).get<int64_t>();
        }

        // Get oblivious table if available
        ObliviousTable* obl_table = nullptr;
        auto it = table_to_oblivious_.find(table);
        if (it != table_to_oblivious_.end()) {
            obl_table = it->second;
        }

        // Perform oblivious recovery for this table
        table_results[table] = obliviousRecoverSingle(table, obl_table, record_ids, col_indices);
    }

    // ========================================================================
    // Phase 2: Build output columns from recovered data
    // Key invariant: results[i] from each table correspond to JOIN pair i
    // Now using direct column moves instead of Value-by-Value construction
    // ========================================================================
    std::vector<Column> output_columns;
    output_columns.reserve(col_map_.size());

    // Track which column index we're at for each table
    std::unordered_map<const Table*, size_t> table_col_cursor;

    for (const auto& mapping : col_map_) {
        const Table* table = mapping.source_table;
        size_t cursor = table_col_cursor[table]++;

        auto& recovery = table_results[table];

        // Direct move of recovered column - no extra copy!
        output_columns.push_back(std::move(recovery.columns[cursor]));
    }

    RowBatch materialized_batch = buildBatchFromColumns(std::move(output_columns));
    setNumRowsForBatch(materialized_batch, num_rows);
    return Result<RowBatch>::success(std::move(materialized_batch));
}

} // namespace velodb
