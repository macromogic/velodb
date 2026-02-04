#include "operator/sort_merge_join_operator.hpp"

#include "common/constants.hpp"
#include "cuda/commands.hpp"
#include "expression/expression.hpp"

#include <cuda_runtime.h>

namespace velodb {

SortMergeJoinOperator::SortMergeJoinOperator(ExecutionContext& context,
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

Result<RowBatch> SortMergeJoinOperator::next()
{
    // Simple single-pass INNER merge equi-join on generic columns
    // Assumptions:
    //  Left & Right inputs are individually sorted ascending by join key
    if (joined_) {
        // Return empty batch to signal completion
        return Result<RowBatch>::success(RowBatch());
    }

    // Collect all left & right batches
    auto left_batch = collectBatches(*left_child_);
    if (left_batch.getRowCount() == 0) {
        // No left rows -> no matches
        joined_ = true;
        return Result<RowBatch>::success(RowBatch());
    }
    auto right_batch = collectBatches(*right_child_);
    if (right_batch.getRowCount() == 0) {
        // No right rows -> no matches
        joined_ = true;
        return Result<RowBatch>::success(RowBatch());
    }

    PROFILE_SCOPE("SortMergeJoin");
    // Assumption: both `left_batch` and `right_batch` are sorted ascending by join key.

    // Validate key indices
    if (join_key_indices_.first >= left_batch.getColumnCount()) {
        return Result<RowBatch>::failure("Left join key index out of bounds");
    }
    if (join_key_indices_.second >= right_batch.getColumnCount()) {
        return Result<RowBatch>::failure("Right join key index out of bounds");
    }

    // Prepare indices for Join phase (0..N-1)
    auto& task_manager = context_.getTaskManager();
    auto stream_handle = StreamPool::getInstance().acquire().value();

    // We use a dummy column to hold indices [0, 1, ... N]
    int64_t* d_left_indices;
    CHECKED_CALL_THROW(
        cudaMallocAsync(&d_left_indices, left_batch.getRowCount() * sizeof(int64_t), stream_handle->get()));
    // We use OP_SORT_MERGE_JOIN_PREPARE to generate sequential indices 0..N ??
    // Wait, PREPARE generates RANDOM indices 0..n_rows.
    // If we want SEQUENTIAL indices, we need a sequence generator.
    // Since we can't change CUDA code, we might need to copy from host or use what we have.
    // Assuming we can copy from host for now (small overhead vs join logic)
    std::vector<int64_t> h_left_indices(left_batch.getRowCount());
    std::iota(h_left_indices.begin(), h_left_indices.end(), 0);
    CHECKED_CALL_THROW(cudaMemcpyAsync(d_left_indices,
                                       h_left_indices.data(),
                                       left_batch.getRowCount() * sizeof(int64_t),
                                       cudaMemcpyHostToDevice,
                                       stream_handle->get()));

    int64_t* d_right_indices;
    CHECKED_CALL_THROW(
        cudaMallocAsync(&d_right_indices, right_batch.getRowCount() * sizeof(int64_t), stream_handle->get()));
    std::vector<int64_t> h_right_indices(right_batch.getRowCount());
    std::iota(h_right_indices.begin(), h_right_indices.end(), 0);
    CHECKED_CALL_THROW(cudaMemcpyAsync(d_right_indices,
                                       h_right_indices.data(),
                                       right_batch.getRowCount() * sizeof(int64_t),
                                       cudaMemcpyHostToDevice,
                                       stream_handle->get()));

    JoinColumn left_join_col { .keys = left_batch.getColumn(join_key_indices_.first).rawData(),
                               .rowids = d_left_indices,
                               .n = left_batch.getRowCount() };
    JoinColumn right_join_col { .keys = right_batch.getColumn(join_key_indices_.second).rawData(),
                                .rowids = d_right_indices,
                                .n = right_batch.getRowCount() };

    // Scan-count phase to determine output size
    // Allocate extra space for join blocks to prevent potential overflow if fragmentation is high
    size_t max_join_blocks = left_batch.getRowCount() * 2;
    MergeJoinBlock* d_join_blocks;
    CHECKED_CALL_THROW(cudaMallocAsync(&d_join_blocks, max_join_blocks * sizeof(MergeJoinBlock), stream_handle->get()));
    size_t* d_join_block_count;
    CHECKED_CALL_THROW(cudaMallocAsync(&d_join_block_count, 1 * sizeof(size_t), stream_handle->get()));
    size_t* d_row_count;
    CHECKED_CALL_THROW(cudaMallocAsync(&d_row_count, 1 * sizeof(size_t), stream_handle->get()));
    CHECKED_CALL_THROW(cudaMemsetAsync(d_join_block_count, 0, sizeof(size_t), stream_handle->get()));
    CHECKED_CALL_THROW(cudaMemsetAsync(d_row_count, 0, sizeof(size_t), stream_handle->get()));
    stream_handle->synchronize();

    Command cmd_count = {};
    cmd_count.opcode = OpCode::OP_SORT_MERGE_JOIN_COUNT;
    cmd_count.args = { .sort_merge_join_count = {
                           .left = left_join_col,
                           .right = right_join_col,
                           .out_blocks = d_join_blocks,
                           .out_block_count = d_join_block_count,
                           .out_row_count = d_row_count,
                           .type_id = left_batch.getColumn(join_key_indices_.first).getType().getTypeId() } };
    task_manager.waitCommand(task_manager.submitCommand(cmd_count));
    size_t h_row_count = 0;
    CHECKED_CALL_THROW(
        cudaMemcpyAsync(&h_row_count, d_row_count, sizeof(size_t), cudaMemcpyDeviceToHost, stream_handle->get()));
    stream_handle->synchronize();
    size_t h_padded_rows = nextPow2(h_row_count);

    // Join-write phase
    // Allocate output indices buffers
    int64_t* d_out_left_indices; // Contains indices into Left Batch
    CHECKED_CALL_THROW(cudaMallocAsync(&d_out_left_indices, h_padded_rows * sizeof(int64_t), stream_handle->get()));
    int64_t* d_out_right_indices; // Contains indices into Right Batch
    CHECKED_CALL_THROW(cudaMallocAsync(&d_out_right_indices, h_padded_rows * sizeof(int64_t), stream_handle->get()));

    // Perform Write (Overwrite hits with correct indices)
    Command cmd_join = {};
    cmd_join.opcode = OpCode::OP_SORT_MERGE_JOIN_WRITE;
    cmd_join.args = { .sort_merge_join_write = {
                          .left = left_join_col,
                          .right = right_join_col,
                          .blocks = d_join_blocks,
                          .n_blocks = d_join_block_count,
                          .out_left = d_out_left_indices,
                          .out_right = d_out_right_indices,
                      } };
    task_manager.waitCommand(task_manager.submitCommand(cmd_join));

    // Create Column wrappers for gathering
    // Note: These columns take ownership of the device pointers, so we DO NOT free d_out_*_indices manually.
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

    // Gather Logic
    std::vector<Column> result_cols;
    result_cols.reserve(left_batch.getColumnCount() + right_batch.getColumnCount());

    size_t last_id = 0;
    // Gather Left Columns
    for (size_t i = 0; i < left_batch.getColumnCount(); ++i) {
        auto& col = left_batch.getColumn(i);
        DataTypeId type_id = col.getType().getTypeId();
        size_t type_size = col.getType().size();

        void* d_out_data;
        CHECKED_CALL_THROW(cudaMallocAsync(&d_out_data, h_padded_rows * type_size, stream_handle->get()));

        Command cmd_permute = {};
        cmd_permute.opcode = OpCode::OP_PERMUTE;
        cmd_permute.args.permute = { .out_data = d_out_data,
                                     .in_data = col.rawData(),
                                     .in_indices = static_cast<const int64_t*>(left_indices_col.rawData()),
                                     .n = h_row_count,
                                     .type_id = type_id };
        last_id = task_manager.submitCommand(cmd_permute);

        // Permute mask (always treated as uint8_t/BOOLEAN)
        void* d_out_mask;
        CHECKED_CALL_THROW(cudaMallocAsync(&d_out_mask, h_padded_rows * sizeof(uint8_t), stream_handle->get()));

        Command cmd_mask = {};
        cmd_mask.opcode = OpCode::OP_PERMUTE;
        cmd_mask.args.permute = { .out_data = d_out_mask,
                                  .in_data = col.rawBitmapData(),
                                  .in_indices = static_cast<const int64_t*>(left_indices_col.rawData()),
                                  .n = h_row_count,
                                  .type_id = DataTypeId::BOOLEAN };
        last_id = task_manager.submitCommand(cmd_mask);

        result_cols.push_back(Column::createFromDeviceBuffers(col.getType().cloneUnique(),
                                                              d_out_data,
                                                              static_cast<uint8_t*>(d_out_mask),
                                                              h_padded_rows,
                                                              h_row_count));
    }

    // Gather Right Columns
    for (size_t i = 0; i < right_batch.getColumnCount(); ++i) {
        auto& col = right_batch.getColumn(i);
        DataTypeId type_id = col.getType().getTypeId();
        size_t type_size = col.getType().size();

        void* d_out_data;
        CHECKED_CALL_THROW(cudaMallocAsync(&d_out_data, h_padded_rows * type_size, stream_handle->get()));

        Command cmd_permute = {};
        cmd_permute.opcode = OpCode::OP_PERMUTE;
        cmd_permute.args.permute = { .out_data = d_out_data,
                                     .in_data = col.rawData(),
                                     .in_indices = static_cast<const int64_t*>(right_indices_col.rawData()),
                                     .n = h_row_count,
                                     .type_id = type_id };
        last_id = task_manager.submitCommand(cmd_permute);

        // Permute mask
        void* d_out_mask;
        CHECKED_CALL_THROW(cudaMallocAsync(&d_out_mask, h_padded_rows * sizeof(uint8_t), stream_handle->get()));

        Command cmd_mask = {};
        cmd_mask.opcode = OpCode::OP_PERMUTE;
        cmd_mask.args.permute = { .out_data = d_out_mask,
                                  .in_data = col.rawBitmapData(),
                                  .in_indices = static_cast<const int64_t*>(right_indices_col.rawData()),
                                  .n = h_row_count,
                                  .type_id = DataTypeId::BOOLEAN };
        last_id = task_manager.submitCommand(cmd_mask);

        result_cols.push_back(Column::createFromDeviceBuffers(col.getType().cloneUnique(),
                                                              d_out_data,
                                                              static_cast<uint8_t*>(d_out_mask),
                                                              h_padded_rows,
                                                              h_row_count));
    }

    task_manager.waitCommand(last_id);

    CHECKED_CALL_THROW(cudaFreeAsync(d_join_blocks, stream_handle->get()));
    CHECKED_CALL_THROW(cudaFreeAsync(d_join_block_count, stream_handle->get()));
    CHECKED_CALL_THROW(cudaFreeAsync(d_row_count, stream_handle->get()));
    CHECKED_CALL_THROW(cudaFreeAsync(d_left_indices, stream_handle->get()));
    CHECKED_CALL_THROW(cudaFreeAsync(d_right_indices, stream_handle->get()));

    RowBatch joined_batch = buildBatchFromColumns(std::move(result_cols));
    setNumRowsForBatch(joined_batch, h_row_count);
    joined_ = true;
    return Result<RowBatch>::success(std::move(joined_batch));
}

} // namespace velodb
