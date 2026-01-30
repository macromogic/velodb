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

Result<RowBatch> HashJoinOperator::next()
{
    PROFILE_SCOPE("HashJoinOperator::next");

    // Already completed - return empty to signal end of stream
    if (joined_) {
        return Result<RowBatch>::success(RowBatch());
    }

    RowBatch build_batch, probe_batch;
    {
        PROFILE_SCOPE("HashJoin: collect build side");
        build_batch = collectBatches(*left_child_);
    }
    if (build_batch.getRowCount() == 0) {
        joined_ = true;
        return Result<RowBatch>::success(RowBatch());
    }
    {
        PROFILE_SCOPE("HashJoin: collect probe side");
        probe_batch = collectBatches(*right_child_);
    }
    if (probe_batch.getRowCount() == 0) {
        joined_ = true;
        return Result<RowBatch>::success(RowBatch());
    }

    // Validate key indices
    if (join_key_indices_.first >= build_batch.getColumnCount()) {
        return Result<RowBatch>::failure("Left join key index out of bounds");
    }
    if (join_key_indices_.second >= probe_batch.getColumnCount()) {
        return Result<RowBatch>::failure("Right join key index out of bounds");
    }

    auto& task_manager = context_.getTaskManager();
    auto stream_handle = StreamPool::getInstance().acquire().value();

    // ========================================================================
    // Step 1: Allocate Hash Table
    // ========================================================================
    size_t build_size = build_batch.getRowCount();
    uint32_t ht_capacity = static_cast<uint32_t>(build_size);
    uint32_t ht_num_buckets = static_cast<uint32_t>(nextPow2(build_size * 2));

    DataTypeId key_type_id = build_batch.getColumn(join_key_indices_.first).getType().getTypeId();

    // Calculate entry size based on key type
    // HashTableEntry<T> = { T key; int64_t rowid; uint32_t next; }
    // IMPORTANT: Must match actual struct sizeof with alignment padding!
    // Struct layout for int32_t key: key(4) + padding(4) + rowid(8) + next(4) + padding(4) = 24
    // Struct layout for int64_t key: key(8) + rowid(8) + next(4) + padding(4) = 24
    size_t entry_size;
    switch (key_type_id) {
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

    void* d_ht_entries;
    uint32_t* d_ht_heads;
    uint32_t* d_ht_counter;

    CHECKED_CALL_THROW(cudaMallocAsync(&d_ht_entries, ht_capacity * entry_size, stream_handle->get()));
    CHECKED_CALL_THROW(cudaMallocAsync(&d_ht_heads, ht_num_buckets * sizeof(uint32_t), stream_handle->get()));
    CHECKED_CALL_THROW(cudaMallocAsync(&d_ht_counter, sizeof(uint32_t), stream_handle->get()));

    // Initialize hash table: entries set to EMPTY pattern, heads = 0xFF (HASH_TABLE_EMPTY), counter = 0
    CHECKED_CALL_THROW(cudaMemsetAsync(d_ht_entries, 0xFF, ht_capacity * entry_size, stream_handle->get()));
    CHECKED_CALL_THROW(cudaMemsetAsync(d_ht_heads, 0xFF, ht_num_buckets * sizeof(uint32_t), stream_handle->get()));
    CHECKED_CALL_THROW(cudaMemsetAsync(d_ht_counter, 0, sizeof(uint32_t), stream_handle->get()));

    // Prepare build side indices [0, 1, ..., N-1]
    int64_t* d_build_indices;
    CHECKED_CALL_THROW(cudaMallocAsync(&d_build_indices, build_size * sizeof(int64_t), stream_handle->get()));
    std::vector<int64_t> h_build_indices(build_size);
    std::iota(h_build_indices.begin(), h_build_indices.end(), 0);
    CHECKED_CALL_THROW(cudaMemcpyAsync(d_build_indices,
                                       h_build_indices.data(),
                                       build_size * sizeof(int64_t),
                                       cudaMemcpyHostToDevice,
                                       stream_handle->get()));
    stream_handle->synchronize();

    // ========================================================================
    // Step 2: Build Hash Table
    // ========================================================================
    Command cmd_build = {};
    cmd_build.opcode = OpCode::OP_HASH_JOIN_BUILD;
    cmd_build.args.hash_join_build = { .keys = build_batch.getColumn(join_key_indices_.first).rawData(),
                                       .rowids = d_build_indices,
                                       .n = build_size,
                                       .ht_entries = d_ht_entries,
                                       .ht_heads = d_ht_heads,
                                       .ht_counter = d_ht_counter,
                                       .ht_capacity = ht_capacity,
                                       .ht_num_buckets = ht_num_buckets,
                                       .type_id = key_type_id };
    task_manager.waitCommand(task_manager.submitCommand(cmd_build));

    // ========================================================================
    // Step 3: Count Matches
    // ========================================================================
    size_t* d_match_count;
    CHECKED_CALL_THROW(cudaMallocAsync(&d_match_count, sizeof(size_t), stream_handle->get()));
    CHECKED_CALL_THROW(cudaMemsetAsync(d_match_count, 0, sizeof(size_t), stream_handle->get()));
    stream_handle->synchronize();

    Command cmd_count = {};
    cmd_count.opcode = OpCode::OP_HASH_JOIN_COUNT;
    cmd_count.args.hash_join_count = { .probe_keys = probe_batch.getColumn(join_key_indices_.second).rawData(),
                                       .probe_n = probe_batch.getRowCount(),
                                       .ht_entries = d_ht_entries,
                                       .ht_heads = d_ht_heads,
                                       .ht_capacity = ht_capacity,
                                       .ht_num_buckets = ht_num_buckets,
                                       .out_count = d_match_count,
                                       .type_id = key_type_id };
    task_manager.waitCommand(task_manager.submitCommand(cmd_count));

    size_t h_match_count = 0;
    CHECKED_CALL_THROW(
        cudaMemcpyAsync(&h_match_count, d_match_count, sizeof(size_t), cudaMemcpyDeviceToHost, stream_handle->get()));
    stream_handle->synchronize();

    if (h_match_count == 0) {
        // No matches - cleanup and return empty
        CHECKED_CALL_THROW(cudaFreeAsync(d_ht_entries, stream_handle->get()));
        CHECKED_CALL_THROW(cudaFreeAsync(d_ht_heads, stream_handle->get()));
        CHECKED_CALL_THROW(cudaFreeAsync(d_ht_counter, stream_handle->get()));
        CHECKED_CALL_THROW(cudaFreeAsync(d_build_indices, stream_handle->get()));
        CHECKED_CALL_THROW(cudaFreeAsync(d_match_count, stream_handle->get()));
        joined_ = true;
        return Result<RowBatch>::success(RowBatch());
    }

    // ========================================================================
    // Step 4: Allocate Output Buffers and Prepare Random Indices (ORAM)
    // ========================================================================
    size_t h_padded_rows = nextPow2(h_match_count);
    int64_t* d_out_left_indices;
    int64_t* d_out_right_indices;
    CHECKED_CALL_THROW(cudaMallocAsync(&d_out_left_indices, h_padded_rows * sizeof(int64_t), stream_handle->get()));
    CHECKED_CALL_THROW(cudaMallocAsync(&d_out_right_indices, h_padded_rows * sizeof(int64_t), stream_handle->get()));

    // Prepare probe side indices [0, 1, ..., N-1]
    int64_t* d_probe_indices;
    CHECKED_CALL_THROW(
        cudaMallocAsync(&d_probe_indices, probe_batch.getRowCount() * sizeof(int64_t), stream_handle->get()));
    std::vector<int64_t> h_probe_indices(probe_batch.getRowCount());
    std::iota(h_probe_indices.begin(), h_probe_indices.end(), 0);
    CHECKED_CALL_THROW(cudaMemcpyAsync(d_probe_indices,
                                       h_probe_indices.data(),
                                       probe_batch.getRowCount() * sizeof(int64_t),
                                       cudaMemcpyHostToDevice,
                                       stream_handle->get()));
    stream_handle->synchronize();

    // Fill output with random valid indices for ORAM padding
    Command cmd_prepare_left = {};
    cmd_prepare_left.opcode = OpCode::OP_SORT_MERGE_JOIN_PREPARE;
    cmd_prepare_left.args.sort_merge_join_prepare
        = { .rowids = d_out_left_indices, .n = h_padded_rows, .n_rows = build_size, .seed = getSeed() };
    task_manager.submitCommand(cmd_prepare_left);

    Command cmd_prepare_right = {};
    cmd_prepare_right.opcode = OpCode::OP_SORT_MERGE_JOIN_PREPARE;
    cmd_prepare_right.args.sort_merge_join_prepare
        = { .rowids = d_out_right_indices, .n = h_padded_rows, .n_rows = probe_batch.getRowCount(), .seed = getSeed() };
    task_manager.waitCommand(task_manager.submitCommand(cmd_prepare_right));

    // ========================================================================
    // Step 5: Write Join Results
    // ========================================================================
    uint32_t* d_write_offset;
    CHECKED_CALL_THROW(cudaMallocAsync(&d_write_offset, sizeof(uint32_t), stream_handle->get()));
    CHECKED_CALL_THROW(cudaMemsetAsync(d_write_offset, 0, sizeof(uint32_t), stream_handle->get()));
    stream_handle->synchronize();

    Command cmd_write = {};
    cmd_write.opcode = OpCode::OP_HASH_JOIN_WRITE;
    cmd_write.args.hash_join_write = { .probe_keys = probe_batch.getColumn(join_key_indices_.second).rawData(),
                                       .probe_rowids = d_probe_indices,
                                       .probe_n = probe_batch.getRowCount(),
                                       .ht_entries = d_ht_entries,
                                       .ht_heads = d_ht_heads,
                                       .ht_capacity = ht_capacity,
                                       .ht_num_buckets = ht_num_buckets,
                                       .out_left = d_out_left_indices,
                                       .out_right = d_out_right_indices,
                                       .write_offset = d_write_offset,
                                       .type_id = key_type_id };
    task_manager.waitCommand(task_manager.submitCommand(cmd_write));

    // ========================================================================
    // Step 6: Gather Columns Using Permute
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
    result_cols.reserve(build_batch.getColumnCount() + probe_batch.getColumnCount());

    size_t last_id = 0;

    // Gather Left (Build) Columns
    for (size_t i = 0; i < build_batch.getColumnCount(); ++i) {
        auto& col = build_batch.getColumn(i);
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

    // ========================================================================
    // Step 7: Cleanup and Return
    // ========================================================================
    CHECKED_CALL_THROW(cudaFreeAsync(d_ht_entries, stream_handle->get()));
    CHECKED_CALL_THROW(cudaFreeAsync(d_ht_heads, stream_handle->get()));
    CHECKED_CALL_THROW(cudaFreeAsync(d_ht_counter, stream_handle->get()));
    CHECKED_CALL_THROW(cudaFreeAsync(d_build_indices, stream_handle->get()));
    CHECKED_CALL_THROW(cudaFreeAsync(d_probe_indices, stream_handle->get()));
    CHECKED_CALL_THROW(cudaFreeAsync(d_match_count, stream_handle->get()));
    CHECKED_CALL_THROW(cudaFreeAsync(d_write_offset, stream_handle->get()));

    // Build result batch (kept on CUDA) and return directly
    RowBatch joined_batch = buildBatchFromColumns(std::move(result_cols));
    setNumRowsForBatch(joined_batch, h_match_count);
    joined_ = true;
    return Result<RowBatch>::success(std::move(joined_batch));
}

} // namespace velodb
