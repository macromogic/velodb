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
    , buffer_(RowBatch::createBuffered(output_schema_, MAX_BATCH_SIZE * 2, DataLocation::CUDA))
{
}

Result<RowBatch> FilterCompactionOperator::next()
{
    auto* child = getChild();
    if (!child) {
        return Result<RowBatch>::failure("FilterCompactionOperator requires a child operator");
    }

    // We assume $_mask always exists
    auto mask_index = output_schema_.getColumnIndex("$_mask");
    while (buffer_.getRowCount() < MAX_BATCH_SIZE) {
        // Execute child operator first
        auto child_result = child->next();
        if (!child_result) {
            return child_result; // Propagate error from child
        }
        auto& input_batch = child_result.value();
        if (input_batch.getRowCount() == 0) {
            break; // No rows to process
        }

        input_batch.to(DataLocation::CUDA);
        buffer_.addFilteredRows(input_batch, input_batch.getColumn(mask_index));
    }

    // Construct the new batch from the buffered data
    auto new_batch = buffer_.splitFront(MAX_BATCH_SIZE);
    return Result<RowBatch>::success(std::move(new_batch));
}

} // namespace velodb
