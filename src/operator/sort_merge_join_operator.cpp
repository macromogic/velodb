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
                                             const Table& left_table,
                                             const Table& right_table,
                                             JoinType join_type)
    : BinaryOperator(context, std::move(output_schema), std::move(left_child), std::move(right_child))
    , left_table_(left_table)
    , right_table_(right_table)
    , join_type_(join_type)
{
}

Result<RowBatch> SortMergeJoinOperator::next()
{
    PROFILE_SCOPE("SortMergeJoinOperator::next");
    // Simple single-pass INNER merge equi-join on first column (join key) of each side.
    // Assumptions:
    //  Left & Right inputs are individually sorted ascending by key (column 0).
    //  Column layout per side after projection: [key, $_rowid, $_mask].
    // Output schema: [left_table_$_rowid, right_table_$_rowid]
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

    // Assumption: both `left_batch` and `right_batch` are sorted ascending by column 0.
    if (left_batch.getColumnCount() < 2 || right_batch.getColumnCount() < 2) {
        return Result<RowBatch>::failure("Input batches must expose key and $_rowid columns at indices 0 and 1");
    }

    JoinColumn left_join_col { .keys = left_batch.getColumn(0).rawData(),
                               .rowids = static_cast<int64_t*>(left_batch.getColumn(1).rawData()),
                               .n = left_batch.getRowCount() };
    JoinColumn right_join_col { .keys = right_batch.getColumn(0).rawData(),
                                .rowids = static_cast<int64_t*>(right_batch.getColumn(1).rawData()),
                                .n = right_batch.getRowCount() };
    auto& task_manager = context_.getTaskManager();

    // Scan-count phase to determine output size
    auto stream_handle = StreamPool::getInstance().acquire().value();
    size_t max_join_blocks = left_batch.getRowCount(); // Allocation must cover worst-case fragmentation
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
    cmd_count.args = { .sort_merge_join_count = { .left = left_join_col,
                                                  .right = right_join_col,
                                                  .out_blocks = d_join_blocks,
                                                  .out_block_count = d_join_block_count,
                                                  .out_row_count = d_row_count,
                                                  .type_id = left_batch.getColumn(0).getType().getTypeId() } };
    task_manager.waitCommand(task_manager.submitCommand(cmd_count));
    size_t h_row_count = 0;
    CHECKED_CALL_THROW(
        cudaMemcpyAsync(&h_row_count, d_row_count, sizeof(size_t), cudaMemcpyDeviceToHost, stream_handle->get()));
    stream_handle->synchronize();
    size_t h_padded_rows = std::max(MIN_PADDING_SIZE, nextPow2(h_row_count));

    // Join-write phase
    int64_t* d_out_left;
    CHECKED_CALL_THROW(cudaMallocAsync(&d_out_left, h_padded_rows * sizeof(int64_t), stream_handle->get()));
    int64_t* d_out_right;
    CHECKED_CALL_THROW(cudaMallocAsync(&d_out_right, h_padded_rows * sizeof(int64_t), stream_handle->get()));
    stream_handle->synchronize();

    Command cmd_join_left = {};
    cmd_join_left.opcode = OpCode::OP_SORT_MERGE_JOIN_PREPARE;
    cmd_join_left.args = { .sort_merge_join_prepare = {
                               .rowids = d_out_left,
                               .n = h_padded_rows,
                               .n_rows = left_table_.get().getRowCount(),
                               .seed = getSeed(),
                           } };
    task_manager.submitCommand(cmd_join_left);
    Command cmd_join_right = {};
    cmd_join_right.opcode = OpCode::OP_SORT_MERGE_JOIN_PREPARE;
    cmd_join_right.args = { .sort_merge_join_prepare = {
                                .rowids = d_out_right,
                                .n = h_padded_rows,
                                .n_rows = right_table_.get().getRowCount(),
                                .seed = getSeed(),
                            } };
    task_manager.waitCommand(task_manager.submitCommand(cmd_join_right));

    Command cmd_join = {};
    cmd_join.opcode = OpCode::OP_SORT_MERGE_JOIN_WRITE;
    // TODO: write random indices for padding region
    cmd_join.args = { .sort_merge_join_write = {
                          .left = left_join_col,
                          .right = right_join_col,
                          .blocks = d_join_blocks,
                          .n_blocks = d_join_block_count,
                          .out_left = d_out_left,
                          .out_right = d_out_right,
                      } };
    task_manager.waitCommand(task_manager.submitCommand(cmd_join));

    Column left_rowid_col(DataType::createType(DataTypeId::BIGINT), h_padded_rows, DataLocation::CUDA);
    Column right_rowid_col(DataType::createType(DataTypeId::BIGINT), h_padded_rows, DataLocation::CUDA);
    left_rowid_col.setFromDeviceBuffers(d_out_left, nullptr);
    right_rowid_col.setFromDeviceBuffers(d_out_right, nullptr);
    std::vector<Column> result_cols;
    result_cols.reserve(2);
    result_cols.push_back(std::move(left_rowid_col));
    result_cols.push_back(std::move(right_rowid_col));
    RowBatch joined_batch = buildBatchFromColumns(std::move(result_cols));
    setNumRowsForBatch(joined_batch, h_row_count);
    joined_ = true;
    return Result<RowBatch>::success(std::move(joined_batch));
}

} // namespace velodb
