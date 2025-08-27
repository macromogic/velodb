#include "operator/filter_compaction_operator.hpp"

#include "catalog/column.hpp"
#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include "common/result.hpp"
#include "data/data_type.hpp"
#include "data/value.hpp"

#include <algorithm>
#include <vector>

#include <cuda_runtime.h>

namespace velodb {

FilterCompactionOperator::FilterCompactionOperator(ExecutionContext& context,
                                                   std::unique_ptr<Schema> output_schema,
                                                   std::unique_ptr<AbstractOperator> child)
    : UnaryOperator(context, std::move(output_schema), std::move(child))
    , num_buffered_rows_(0)
{
    size_t num_columns = output_schema_->getColumnCount();
    device_buffers_.reserve(num_columns);
    for (size_t i = 0; i < num_columns; ++i) {
        auto& type = output_schema_->getColumnInfo(i).getType();
        void* buffer;
        cudaMalloc(&buffer, type.size() * MAX_BATCH_SIZE * 2);
        device_buffers_.emplace_back(buffer);
    }
}

FilterCompactionOperator::~FilterCompactionOperator()
{
    for (void* buffer : device_buffers_) {
        cudaFree(buffer);
    }
}

Result<View> FilterCompactionOperator::next()
{
    auto* child = getChild();
    if (!child) {
        return Result<View>::failure("FilterCompactionOperator requires a child operator");
    }

    // Execute child operator first
    auto child_result = child->next();
    if (!child_result) {
        return child_result; // Propagate error from child
    }
    const auto& input_view = child_result.value();
    if (input_view.getRowCount() == 0) {
        return child_result; // No rows to process
    }
    const auto& schema = input_view.getSchema();
    if (!schema.hasColumn("$_mask")) {
        return child_result;
    }

    auto table_info = std::make_unique<TableInfo>(input_view.getTableInfo().getName(), output_schema_->cloneUnique());
    std::vector<std::reference_wrapper<ValueColumn>> columns;
    auto column_count = output_schema_->getColumnCount();
    columns.reserve(column_count);
    for (size_t i = 0; i < column_count; ++i) {
        const auto& column_info = output_schema_->getColumnInfo(i);
        columns.push_back(context_.createTemporaryColumn(column_info.getName(), column_info.getType().cloneUnique()));
    }
    for (const auto& tuple : input_view) {
        // Check if the row should be included based on $_mask
        const Value& mask_value = tuple.getValue("$_mask");
        if (mask_value.isNull() || !mask_value.getBoolean()) {
            continue; // Skip rows where $_mask is false
        }

        // Append values to the output columns
        for (size_t i = 0; i < column_count; ++i) {
            Value value = tuple.getValue(i);
            columns[i].get().append(value);
        }
    }

    std::vector<ViewColumn> view_columns;
    view_columns.reserve(column_count);
    const auto& mask_column = input_view.getColumn("$_mask");
    for (size_t i = 0; i < column_count; ++i) {
        // const auto& column_info = output_schema_->getColumnInfo(i);
        // TODO: do stuff on gpu
    }
    for (auto& column : columns) {
        view_columns.emplace_back(column.get().view());
    }
    View view(std::move(table_info), std::move(view_columns));
    return Result<View>::success(std::move(view));
}

} // namespace velodb
