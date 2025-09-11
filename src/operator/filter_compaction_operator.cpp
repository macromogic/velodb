#include "operator/filter_compaction_operator.hpp"

#include "catalog/column.hpp"
#include "catalog/row_batch.hpp"
#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include "common/result.hpp"
#include "data/data_type.hpp"

#include <vector>

#include <cuda_runtime.h>

namespace velodb {

FilterCompactionOperator::FilterCompactionOperator(ExecutionContext& context,
                                                   Schema output_schema,
                                                   std::unique_ptr<AbstractOperator> child)
    : UnaryOperator(context, std::move(output_schema), std::move(child))
    , num_buffered_rows_(0)
{
    size_t num_columns = output_schema_.getColumnCount();
    device_buffers_.reserve(num_columns);
    for (size_t i = 0; i < num_columns; ++i) {
        auto& type = output_schema_.getColumnInfo(i).getType();
        device_buffers_.emplace_back(type.cloneUnique(), MAX_BATCH_SIZE * 2, DataLocation::CUDA);
    }
}

Result<RowBatch> FilterCompactionOperator::next()
{
    auto* child = getChild();
    if (!child) {
        return Result<RowBatch>::failure("FilterCompactionOperator requires a child operator");
    }
    // We assume $_mask always exists
    auto mask_index = output_schema_.getColumnIndex("$_mask");
    auto column_count = output_schema_.getColumnCount();

    while (num_buffered_rows_ < MAX_BATCH_SIZE) {
        // Execute child operator first
        auto child_result = child->next();
        if (!child_result) {
            return child_result; // Propagate error from child
        }
        auto& input_batch = child_result.value();
        if (input_batch.getRowCount() == 0) {
            break; // No rows to process
        }
        size_t batch_size = input_batch.getRowCount();
        auto& mask_column = input_batch.getColumn(mask_index);
        for (size_t i = 0; i < batch_size; ++i) {
        }

        input_batch.to(DataLocation::CUDA);
        input_batch.compact(device_buffers_, mask_index);
        num_buffered_rows_ = device_buffers_.front().size();
        /*
        auto& mask_column = input_batch.getColumn(mask_index);
        for (size_t i = 0; i < batch_size; ++i) {
            if (mask_column.get(i).getBoolean()) {
                for (size_t j = 0; j < column_count; ++j) {
                    device_buffers_[j].append(input_batch.getValue(i, j));
                }
                ++num_buffered_rows_;
            }
        }
        */
    }

    // Construct the new batch from the buffered data
    auto new_batch = RowBatch();
    for (size_t j = 0; j < column_count; ++j) {
        new_batch.addColumn(device_buffers_[j].splitFront(MAX_BATCH_SIZE));
    }
    return Result<RowBatch>::success(std::move(new_batch));
}

} // namespace velodb
