#include "operator/hash_join_operator.hpp"

#include "common/constants.hpp"
#include "cuda/commands.hpp"
#include "cuda/hash_table.hpp"
#include "expression/expression.hpp"

#include <numeric>

#include <cuda_runtime.h>

namespace velodb {

HashJoinOperator::HashJoinOperator(ExecutionContext& context,
                                   Schema output_schema,
                                   std::unique_ptr<AbstractOperator> left_child,
                                   std::unique_ptr<AbstractOperator> right_child,
                                   std::pair<size_t, size_t> join_key_indices,
                                   std::vector<const Table*> left_source_tables,
                                   std::vector<const Table*> right_source_tables,
                                   JoinType join_type)
    : BinaryOperator(context, std::move(output_schema), std::move(left_child), std::move(right_child))
    , join_key_indices_(join_key_indices)
    , left_source_tables_(std::move(left_source_tables))
    , right_source_tables_(std::move(right_source_tables))
    , join_type_(join_type)
{
}

HashJoinOperator::~HashJoinOperator()
{
    cleanup();
}

void HashJoinOperator::cleanup()
{
    if (d_ht_entries_ || d_ht_heads_) {
        auto stream_handle = StreamPool::getInstance().acquire().value();
        if (d_ht_entries_) {
            cudaFreeAsync(d_ht_entries_, stream_handle->get());
            d_ht_entries_ = nullptr;
        }
        if (d_ht_heads_) {
            cudaFreeAsync(d_ht_heads_, stream_handle->get());
            d_ht_heads_ = nullptr;
        }
        stream_handle->synchronize();
    }
}

// ============================================================================
// Build Phase: Collect build side and construct hash table
// ============================================================================

bool HashJoinOperator::buildHashTable()
{
    PROFILE_SCOPE("HashJoin: build hash table");

    // Collect all build side data
    {
        PROFILE_SCOPE("HashJoin: collect build side");
        build_batch_ = collectBatches(*left_child_);
    }

    build_size_ = build_batch_.getRowCount();
    if (build_size_ == 0) {
        return false; // No data to join
    }

    // Validate key index
    if (join_key_indices_.first >= build_batch_.getColumnCount()) {
        VELODB_THROW(ExecutionError, "Left join key index out of bounds");
    }

    key_type_id_ = build_batch_.getColumn(join_key_indices_.first).getType().getTypeId();

    auto& task_manager = context_.getTaskManager();
    auto stream_handle = StreamPool::getInstance().acquire().value();

    // ========================================================================
    // Step 1: Allocate Hash Table
    // ========================================================================
    ht_capacity_ = static_cast<uint32_t>(build_size_);
    ht_num_buckets_ = static_cast<uint32_t>(nextPow2(build_size_ * 2));

    // Calculate entry size based on key type
    size_t entry_size;
    switch (key_type_id_) {
    case DataTypeId::INTEGER:
        entry_size = sizeof(HashTableEntry<int32_t>);
        break;
    case DataTypeId::BIGINT:
        entry_size = sizeof(HashTableEntry<int64_t>);
        break;
    case DataTypeId::DOUBLE:
        entry_size = sizeof(HashTableEntry<double>);
        break;
    default:
        VELODB_THROW(ExecutionError, "Unsupported key type for Hash Join");
    }

    CHECKED_CALL_THROW(cudaMalloc(&d_ht_entries_, ht_capacity_ * entry_size));
    CHECKED_CALL_THROW(cudaMalloc(&d_ht_heads_, ht_num_buckets_ * sizeof(uint32_t)));

    uint32_t* d_ht_counter;
    CHECKED_CALL_THROW(cudaMallocAsync(&d_ht_counter, sizeof(uint32_t), stream_handle->get()));

    // Initialize hash table
    CHECKED_CALL_THROW(cudaMemsetAsync(d_ht_entries_, 0xFF, ht_capacity_ * entry_size, stream_handle->get()));
    CHECKED_CALL_THROW(cudaMemsetAsync(d_ht_heads_, 0xFF, ht_num_buckets_ * sizeof(uint32_t), stream_handle->get()));
    CHECKED_CALL_THROW(cudaMemsetAsync(d_ht_counter, 0, sizeof(uint32_t), stream_handle->get()));

    // Prepare build side indices [0, 1, ..., N-1]
    int64_t* d_build_indices;
    CHECKED_CALL_THROW(cudaMallocAsync(&d_build_indices, build_size_ * sizeof(int64_t), stream_handle->get()));
    std::vector<int64_t> h_build_indices(build_size_);
    std::iota(h_build_indices.begin(), h_build_indices.end(), 0);
    CHECKED_CALL_THROW(cudaMemcpyAsync(d_build_indices,
                                       h_build_indices.data(),
                                       build_size_ * sizeof(int64_t),
                                       cudaMemcpyHostToDevice,
                                       stream_handle->get()));
    stream_handle->synchronize();

    // ========================================================================
    // Step 2: Build Hash Table
    // ========================================================================
    Command cmd_build = {};
    cmd_build.opcode = OpCode::OP_HASH_JOIN_BUILD;
    cmd_build.args.hash_join_build = { .keys = build_batch_.getColumn(join_key_indices_.first).rawData(),
                                       .rowids = d_build_indices,
                                       .n = build_size_,
                                       .ht_entries = d_ht_entries_,
                                       .ht_heads = d_ht_heads_,
                                       .ht_counter = d_ht_counter,
                                       .ht_capacity = ht_capacity_,
                                       .ht_num_buckets = ht_num_buckets_,
                                       .type_id = key_type_id_ };
    task_manager.waitCommand(task_manager.submitCommand(cmd_build));

    // Cleanup temporary buffers
    CHECKED_CALL_THROW(cudaFreeAsync(d_build_indices, stream_handle->get()));
    CHECKED_CALL_THROW(cudaFreeAsync(d_ht_counter, stream_handle->get()));
    stream_handle->synchronize();

    return true; // Hash table built successfully
}

// ============================================================================
// Probe Phase: Process a single probe batch against the hash table
// ============================================================================

Result<RowBatch> HashJoinOperator::probeWithBatch(RowBatch& probe_batch)
{
    PROFILE_SCOPE("HashJoin: probe batch");

    size_t probe_n = probe_batch.getRowCount();
    if (probe_n == 0) {
        return Result<RowBatch>::success(RowBatch());
    }

    auto& task_manager = context_.getTaskManager();
    auto stream_handle = StreamPool::getInstance().acquire().value();

    // ========================================================================
    // Step 1: Count matches for this probe batch
    // ========================================================================
    size_t* d_match_count;
    CHECKED_CALL_THROW(cudaMallocAsync(&d_match_count, sizeof(size_t), stream_handle->get()));
    CHECKED_CALL_THROW(cudaMemsetAsync(d_match_count, 0, sizeof(size_t), stream_handle->get()));
    stream_handle->synchronize();

    Command cmd_count = {};
    cmd_count.opcode = OpCode::OP_HASH_JOIN_COUNT;
    cmd_count.args.hash_join_count = { .probe_keys = probe_batch.getColumn(join_key_indices_.second).rawData(),
                                       .probe_n = probe_n,
                                       .ht_entries = d_ht_entries_,
                                       .ht_heads = d_ht_heads_,
                                       .ht_capacity = ht_capacity_,
                                       .ht_num_buckets = ht_num_buckets_,
                                       .out_count = d_match_count,
                                       .type_id = key_type_id_ };
    task_manager.waitCommand(task_manager.submitCommand(cmd_count));

    size_t h_match_count = 0;
    CHECKED_CALL_THROW(
        cudaMemcpyAsync(&h_match_count, d_match_count, sizeof(size_t), cudaMemcpyDeviceToHost, stream_handle->get()));
    stream_handle->synchronize();
    CHECKED_CALL_THROW(cudaFreeAsync(d_match_count, stream_handle->get()));

    if (h_match_count == 0) {
        // No matches for this batch - return empty but continue probing
        return Result<RowBatch>::success(RowBatch());
    }

    // ========================================================================
    // Step 2: Allocate output buffers (padded to power of 2 for ORAM)
    // ========================================================================
    size_t h_padded_rows = nextPow2(h_match_count);
    int64_t* d_out_left_indices;
    int64_t* d_out_right_indices;
    CHECKED_CALL_THROW(cudaMallocAsync(&d_out_left_indices, h_padded_rows * sizeof(int64_t), stream_handle->get()));
    CHECKED_CALL_THROW(cudaMallocAsync(&d_out_right_indices, h_padded_rows * sizeof(int64_t), stream_handle->get()));

    // Prepare probe side indices [0, 1, ..., probe_n-1]
    int64_t* d_probe_indices;
    CHECKED_CALL_THROW(cudaMallocAsync(&d_probe_indices, probe_n * sizeof(int64_t), stream_handle->get()));
    std::vector<int64_t> h_probe_indices(probe_n);
    std::iota(h_probe_indices.begin(), h_probe_indices.end(), 0);
    CHECKED_CALL_THROW(cudaMemcpyAsync(d_probe_indices,
                                       h_probe_indices.data(),
                                       probe_n * sizeof(int64_t),
                                       cudaMemcpyHostToDevice,
                                       stream_handle->get()));
    stream_handle->synchronize();

    // Fill output with random valid indices for ORAM padding
    Command cmd_prepare_left = {};
    cmd_prepare_left.opcode = OpCode::OP_SORT_MERGE_JOIN_PREPARE;
    cmd_prepare_left.args.sort_merge_join_prepare
        = { .rowids = d_out_left_indices, .n = h_padded_rows, .n_rows = build_size_, .seed = getSeed() };
    task_manager.submitCommand(cmd_prepare_left);

    Command cmd_prepare_right = {};
    cmd_prepare_right.opcode = OpCode::OP_SORT_MERGE_JOIN_PREPARE;
    cmd_prepare_right.args.sort_merge_join_prepare
        = { .rowids = d_out_right_indices, .n = h_padded_rows, .n_rows = probe_n, .seed = getSeed() };
    task_manager.waitCommand(task_manager.submitCommand(cmd_prepare_right));

    // ========================================================================
    // Step 3: Write join results
    // ========================================================================
    uint32_t* d_write_offset;
    CHECKED_CALL_THROW(cudaMallocAsync(&d_write_offset, sizeof(uint32_t), stream_handle->get()));
    CHECKED_CALL_THROW(cudaMemsetAsync(d_write_offset, 0, sizeof(uint32_t), stream_handle->get()));
    stream_handle->synchronize();

    Command cmd_write = {};
    cmd_write.opcode = OpCode::OP_HASH_JOIN_WRITE;
    cmd_write.args.hash_join_write = { .probe_keys = probe_batch.getColumn(join_key_indices_.second).rawData(),
                                       .probe_rowids = d_probe_indices,
                                       .probe_n = probe_n,
                                       .ht_entries = d_ht_entries_,
                                       .ht_heads = d_ht_heads_,
                                       .ht_capacity = ht_capacity_,
                                       .ht_num_buckets = ht_num_buckets_,
                                       .out_left = d_out_left_indices,
                                       .out_right = d_out_right_indices,
                                       .write_offset = d_write_offset,
                                       .type_id = key_type_id_ };
    task_manager.waitCommand(task_manager.submitCommand(cmd_write));

    CHECKED_CALL_THROW(cudaFreeAsync(d_probe_indices, stream_handle->get()));
    CHECKED_CALL_THROW(cudaFreeAsync(d_write_offset, stream_handle->get()));

    // ========================================================================
    // Step 4: Gather columns using permute
    // ========================================================================
    auto left_indices_col = Column::createFromDeviceBuffers(DataType::createType(DataTypeId::BIGINT),
                                                            d_out_left_indices,
                                                            nullptr,
                                                            h_padded_rows,
                                                            h_padded_rows);

    auto right_indices_col = Column::createFromDeviceBuffers(DataType::createType(DataTypeId::BIGINT),
                                                             d_out_right_indices,
                                                             nullptr,
                                                             h_padded_rows,
                                                             h_padded_rows);

    std::vector<Column> result_cols;
    result_cols.reserve(build_batch_.getColumnCount() + probe_batch.getColumnCount());

    size_t last_id = 0;

    // Gather Left (Build) Columns
    for (size_t i = 0; i < build_batch_.getColumnCount(); ++i) {
        auto& col = build_batch_.getColumn(i);
        DataTypeId type_id = col.getType().getTypeId();
        size_t type_size = col.getType().size();

        void* d_out_data;
        CHECKED_CALL_THROW(cudaMallocAsync(&d_out_data, h_padded_rows * type_size, stream_handle->get()));

        Command cmd_permute = {};
        cmd_permute.opcode = OpCode::OP_PERMUTE;
        cmd_permute.args.permute = { .out_data = d_out_data,
                                     .in_data = col.rawData(),
                                     .in_indices = static_cast<const int64_t*>(left_indices_col.rawData()),
                                     .n = h_padded_rows,
                                     .type_id = type_id };
        last_id = task_manager.submitCommand(cmd_permute);

        void* d_out_mask;
        CHECKED_CALL_THROW(cudaMallocAsync(&d_out_mask, h_padded_rows * sizeof(uint8_t), stream_handle->get()));

        Command cmd_mask = {};
        cmd_mask.opcode = OpCode::OP_PERMUTE;
        cmd_mask.args.permute = { .out_data = d_out_mask,
                                  .in_data = col.rawBitmapData(),
                                  .in_indices = static_cast<const int64_t*>(left_indices_col.rawData()),
                                  .n = h_padded_rows,
                                  .type_id = DataTypeId::BOOLEAN };
        last_id = task_manager.submitCommand(cmd_mask);

        result_cols.push_back(Column::createFromDeviceBuffers(col.getType().cloneUnique(),
                                                              d_out_data,
                                                              static_cast<uint8_t*>(d_out_mask),
                                                              h_match_count,
                                                              h_padded_rows));
    }

    // Gather Right (Probe) Columns
    for (size_t i = 0; i < probe_batch.getColumnCount(); ++i) {
        auto& col = probe_batch.getColumn(i);
        DataTypeId type_id = col.getType().getTypeId();
        size_t type_size = col.getType().size();

        void* d_out_data;
        CHECKED_CALL_THROW(cudaMallocAsync(&d_out_data, h_padded_rows * type_size, stream_handle->get()));

        Command cmd_permute = {};
        cmd_permute.opcode = OpCode::OP_PERMUTE;
        cmd_permute.args.permute = { .out_data = d_out_data,
                                     .in_data = col.rawData(),
                                     .in_indices = static_cast<const int64_t*>(right_indices_col.rawData()),
                                     .n = h_padded_rows,
                                     .type_id = type_id };
        last_id = task_manager.submitCommand(cmd_permute);

        void* d_out_mask;
        CHECKED_CALL_THROW(cudaMallocAsync(&d_out_mask, h_padded_rows * sizeof(uint8_t), stream_handle->get()));

        Command cmd_mask = {};
        cmd_mask.opcode = OpCode::OP_PERMUTE;
        cmd_mask.args.permute = { .out_data = d_out_mask,
                                  .in_data = col.rawBitmapData(),
                                  .in_indices = static_cast<const int64_t*>(right_indices_col.rawData()),
                                  .n = h_padded_rows,
                                  .type_id = DataTypeId::BOOLEAN };
        last_id = task_manager.submitCommand(cmd_mask);

        result_cols.push_back(Column::createFromDeviceBuffers(col.getType().cloneUnique(),
                                                              d_out_data,
                                                              static_cast<uint8_t*>(d_out_mask),
                                                              h_match_count,
                                                              h_padded_rows));
    }

    task_manager.waitCommand(last_id);

    // Build result batch
    RowBatch joined_batch = buildBatchFromColumns(std::move(result_cols));
    setNumRowsForBatch(joined_batch, h_match_count);
    return Result<RowBatch>::success(std::move(joined_batch));
}

// ============================================================================
// Main Entry Point: Streaming hash join
// ============================================================================

Result<RowBatch> HashJoinOperator::next()
{
    PROFILE_SCOPE("HashJoinOperator::next");

    // Already finished - return empty batch
    if (finished_) {
        return Result<RowBatch>::success(RowBatch());
    }

    // Build hash table on first call
    if (!hash_table_built_) {
        if (!buildHashTable()) {
            // Empty build side - no results possible
            finished_ = true;
            return Result<RowBatch>::success(RowBatch());
        }
        hash_table_built_ = true;
    }

    // Stream probe batches from right child
    while (true) {
        PROFILE_SCOPE("HashJoin: collect probe side");
        auto probe_result = right_child_->next();
        if (!probe_result) {
            return probe_result;
        }

        auto probe_batch = std::move(probe_result.value());
        if (probe_batch.getRowCount() == 0) {
            // No more probe batches - we're done
            finished_ = true;
            cleanup();
            return Result<RowBatch>::success(RowBatch());
        }

        // Validate key index
        if (join_key_indices_.second >= probe_batch.getColumnCount()) {
            return Result<RowBatch>::failure("Right join key index out of bounds");
        }

        // Probe hash table with this batch
        auto join_result = probeWithBatch(probe_batch);
        if (!join_result) {
            return join_result;
        }

        auto joined_batch = std::move(join_result.value());
        if (joined_batch.getRowCount() > 0) {
            // Return this batch of results
            return Result<RowBatch>::success(std::move(joined_batch));
        }
        // No matches for this probe batch, continue to next
    }
}

} // namespace velodb
