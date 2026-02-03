#include "operator/gpu_filter_operator.hpp"

#include "catalog/column.hpp"
#include "catalog/row_batch.hpp"
#include "catalog/schema.hpp"
#include "common/constants.hpp"
#include "common/profiler.hpp"
#include "common/result.hpp"
#include "cuda/commands.hpp"
#include "cuda/stream_pool.hpp"
#include "expression/column_ref_expression.hpp"
#include "expression/comparison_expression.hpp"

#include <cuda_runtime.h>

namespace velodb {

GpuFilterOperator::GpuFilterOperator(ExecutionContext& context,
                                     Schema output_schema,
                                     std::unique_ptr<AbstractOperator> child,
                                     std::unique_ptr<AbstractExpression> predicate)
    : UnaryOperator(context, std::move(output_schema), std::move(child))
    , predicate_(std::move(predicate))
{
}

Result<RowBatch> GpuFilterOperator::next()
{
    auto child_result = child_->next();
    if (!child_result) {
        return child_result;
    }

    auto& batch = child_result.value();
    if (batch.getRowCount() == 0) {
        return child_result; // End of stream
    }

    PROFILE_SCOPE("Post-join Filter");
    batch.to(DataLocation::CUDA);
    size_t n_rows = batch.getRowCount();

    auto& task_manager = context_.getTaskManager();
    auto stream_handle = StreamPool::getInstance().acquire().value();

    // Step 1: Evaluate predicate to get mask
    VELODB_ASSERT_MSG(predicate_->getExpressionType() == ExpressionType::COMPARISON,
                      "GpuFilterOperator only supports comparison predicates");

    auto* compare = static_cast<const ComparisonExpression*>(predicate_.get());
    VELODB_ASSERT_MSG(compare->getComparisonType() == ComparisonType::EQUAL,
                      "GpuFilterOperator only supports EQUAL comparisons for now");

    auto* left_expr = &compare->getLeftExpression();
    auto* right_expr = &compare->getRightExpression();

    VELODB_ASSERT_MSG(left_expr->getExpressionType() == ExpressionType::COLUMN_REF,
                      "Left side must be column reference");
    VELODB_ASSERT_MSG(right_expr->getExpressionType() == ExpressionType::COLUMN_REF,
                      "Right side must be column reference");

    auto* left_col_ref = static_cast<const ColumnRefExpression*>(left_expr);
    auto* right_col_ref = static_cast<const ColumnRefExpression*>(right_expr);

    // Find column indices in the batch
    size_t left_idx = output_schema_.getColumnIndex(left_col_ref->getColumnName());
    size_t right_idx = output_schema_.getColumnIndex(right_col_ref->getColumnName());

    auto& left_col = batch.getColumn(left_idx);
    auto& right_col = batch.getColumn(right_idx);

    DataTypeId type_id = left_col.getType().getTypeId();
    VELODB_ASSERT_MSG(type_id == right_col.getType().getTypeId(), "Column types must match for comparison");

    // Step 2: Allocate mask buffer on GPU and compute comparison
    uint8_t* d_mask;
    CHECKED_CALL_THROW(cudaMallocAsync(&d_mask, n_rows * sizeof(uint8_t), stream_handle->get()));
    CHECKED_CALL_THROW(cudaMemsetAsync(d_mask, 0, n_rows * sizeof(uint8_t), stream_handle->get()));
    stream_handle->synchronize();

    // Use OP_COMPARE command to compute equality mask
    Command compare_cmd = {};
    compare_cmd.opcode = OpCode::OP_COMPARE_EQ;
    compare_cmd.args.compare_eq = {
        .left_data = left_col.rawData(),
        .right_data = right_col.rawData(),
        .out_mask = d_mask,
        .n = n_rows,
        .type_id = type_id,
    };
    task_manager.waitCommand(task_manager.submitCommand(compare_cmd));

    // Step 3: Use scatter to compute valid row indices
    int32_t* d_scatter_indices;
    CHECKED_CALL_THROW(cudaMallocAsync(&d_scatter_indices, n_rows * sizeof(int32_t), stream_handle->get()));
    size_t* d_scatter_count;
    CHECKED_CALL_THROW(cudaMallocAsync(&d_scatter_count, sizeof(size_t), stream_handle->get()));
    CHECKED_CALL_THROW(cudaMemsetAsync(d_scatter_count, 0, sizeof(size_t), stream_handle->get()));
    stream_handle->synchronize();

    Command scatter_cmd = {};
    scatter_cmd.opcode = OpCode::OP_SCATTER;
    scatter_cmd.args.scatter = {
        .out_indices = d_scatter_indices,
        .out_count = d_scatter_count,
        .in_mask = d_mask,
        .n = n_rows,
    };
    task_manager.waitCommand(task_manager.submitCommand(scatter_cmd));

    // Get scatter count
    size_t h_scatter_count = 0;
    CHECKED_CALL_THROW(cudaMemcpyAsync(&h_scatter_count,
                                       d_scatter_count,
                                       sizeof(size_t),
                                       cudaMemcpyDeviceToHost,
                                       stream_handle->get()));
    stream_handle->synchronize();

    // Step 4: Gather valid rows for each column
    // Process columns one at a time to reduce peak GPU memory usage
    uint64_t last_id = 0;
    size_t n_cols = batch.getColumnCount();

    for (size_t col_idx = 0; col_idx < n_cols; ++col_idx) {
        auto& input_col = batch.getColumn(col_idx);
        auto* data_ptr = input_col.rawData();
        auto* bitmap_ptr = input_col.rawBitmapData();
        auto* temp_buffer = input_col.getDeviceBuffer();
        auto* temp_bitmap_buffer = input_col.getDeviceBitmapBuffer();

        Command gather_cmd = {};
        gather_cmd.opcode = OpCode::OP_GATHER;
        gather_cmd.args.gather = {
            .out_data = temp_buffer,
            .in_data = data_ptr,
            .in_indices = d_scatter_indices,
            .in_mask = d_mask,
            .n = n_rows,
            .type_id = input_col.getType().getTypeId(),
        };
        task_manager.submitCommand(gather_cmd);

        Command gather_bits_cmd = {};
        gather_bits_cmd.opcode = OpCode::OP_GATHER;
        gather_bits_cmd.args.gather = {
            .out_data = temp_bitmap_buffer,
            .in_data = bitmap_ptr,
            .in_indices = d_scatter_indices,
            .in_mask = d_mask,
            .n = n_rows,
            .type_id = DataTypeId::BOOLEAN,
        };
        last_id = task_manager.submitCommand(gather_bits_cmd);

        // Wait and swap buffers immediately to free old GPU memory
        task_manager.waitCommand(last_id);
        input_col.setFromDeviceBuffers(temp_buffer, temp_bitmap_buffer);
    }
    setNumRowsForBatch(batch, h_scatter_count);

    // Step 6: Cleanup
    CHECKED_CALL_THROW(cudaFreeAsync(d_mask, stream_handle->get()));
    CHECKED_CALL_THROW(cudaFreeAsync(d_scatter_indices, stream_handle->get()));
    CHECKED_CALL_THROW(cudaFreeAsync(d_scatter_count, stream_handle->get()));

    return Result<RowBatch>::success(std::move(batch));
}

} // namespace velodb
