#include "operator/abstract_operator.hpp"

#include "catalog/execution_context.hpp"

#include <deque>

namespace velodb {

// AbstractOperator implementation
AbstractOperator::AbstractOperator(ExecutionContext& context, Schema output_schema)
    : context_(context)
    , output_schema_(std::move(output_schema))
{
}

RowBatch AbstractOperator::collectBatches(AbstractOperator& child)
{
    std::deque<RowBatch> buffer;
    size_t n_rows = 0;
    while (true) {
        auto result = child.next();
        if (!result) {
            VELODB_THROW(ExecutionError, fmt::format("Error fetching batch from child operator: {}", result.error()));
        }
        auto& batch = result.value();
        if (batch.getRowCount() == 0) {
            break; // End of stream
        }
        n_rows += batch.getRowCount();
        buffer.push_back(std::move(batch));
    }

    if (buffer.empty()) {
        return RowBatch();
    }
    size_t capacity = nextPow2(n_rows);
    RowBatch gathered_batch = std::move(buffer.front());
    buffer.pop_front();
    for (auto& col : gathered_batch.getColumns()) {
        col.reserve(capacity);
    }
    size_t n_cols = gathered_batch.getColumnCount();
    // auto& task_manager = context_.getTaskManager();
    if (!buffer.empty()) {
        auto stream_handler = StreamPool::getInstance().acquire().value();
        // uint64_t last_id;
        size_t offset = 0;
        for (const auto& batch : buffer) {
            size_t batch_rows = batch.getRowCount();
            for (size_t col_idx = 0; col_idx < n_cols; ++col_idx) {
                auto& dest_col = gathered_batch.getColumn(col_idx);
                auto& src_col = batch.getColumn(col_idx);
                auto& col_type = dest_col.getType();

                auto* dest_data = static_cast<uint8_t*>(dest_col.rawData()) + offset * col_type.size();
                auto* src_data = src_col.rawData();
                CHECKED_CALL_THROW(cudaMemcpyAsync(dest_data,
                                                   src_data,
                                                   batch_rows * col_type.size(),
                                                   cudaMemcpyDeviceToDevice,
                                                   stream_handler->get()));

                auto* dest_bitmap = dest_col.rawBitmapData();
                auto* src_bitmap = src_col.rawBitmapData();

                CHECKED_CALL_THROW(cudaMemcpyAsync(dest_bitmap + offset,
                                                   src_bitmap,
                                                   batch_rows * sizeof(BitVector::Element),
                                                   cudaMemcpyDeviceToDevice,
                                                   stream_handler->get()));
            }
            offset += batch_rows;
        }
        stream_handler->synchronize();
        // task_manager.waitCommand(last_id);
    }

    setNumRowsForBatch(gathered_batch, n_rows);
    return gathered_batch;
}

UnaryOperator::UnaryOperator(ExecutionContext& context, Schema output_schema, std::unique_ptr<AbstractOperator> child)
    : AbstractOperator(context, std::move(output_schema))
    , child_(std::move(child))
{
}

BinaryOperator::BinaryOperator(ExecutionContext& context,
                               Schema output_schema,
                               std::unique_ptr<AbstractOperator> left_child,
                               std::unique_ptr<AbstractOperator> right_child)
    : AbstractOperator(context, std::move(output_schema))
    , left_child_(std::move(left_child))
    , right_child_(std::move(right_child))
{
}

} // namespace velodb
