#include "operator/compaction_operator.hpp"
#include "catalog/table.hpp"
#include "catalog/column.hpp"
#include "catalog/schema.hpp"
#include "types/value.hpp"
#include "types/data_type.hpp"
#include "common/result.hpp"
#include <vector>
#include <algorithm>
#include <iostream>

namespace velodb {

CompactionOperator::CompactionOperator(Catalog& catalog,
                                     std::unique_ptr<Schema> output_schema,
                                     std::unique_ptr<AbstractOperator> child)
    : UnaryOperator(catalog, std::move(output_schema), std::move(child)) {
}

Result<View> CompactionOperator::execute() const {
    auto* child = getChild();
    if (!child) {
        return Result<View>::failure("CompactionOperator requires a child operator");
    }

    // Execute child operator first
    auto child_result = child->execute();
    if (!child_result) {
        return child_result; // Propagate error from child
    }
    const auto& input_view = child_result.value();
    // std::cout << "input_view: " << input_view.toString() << std::endl;
    const auto& schema = input_view.getTableInfo().getSchema();
    bool has_mask_column = schema.hasColumn("$_mask");
    if (!has_mask_column) {
        return child_result;
    }

    auto table_info = std::make_unique<TableInfo>(input_view.getTableInfo().getName(), output_schema_->cloneUnique());
    std::vector<std::reference_wrapper<ValueColumn>> columns;
    auto column_count = output_schema_->getColumnCount();
    columns.reserve(column_count);
    for (size_t i = 0; i < column_count; ++i) {
        const auto& column_info = output_schema_->getColumnInfo(i);
        columns.push_back(catalog_.createTemporaryColumn(column_info.getName(), column_info.getType().cloneUnique()));
    }
    for (const auto& tuple : input_view) {
        // std::cout << "Scan: " << tuple.toString() << std::endl;
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
    for (auto& column : columns) {
        view_columns.emplace_back(column.get().view());
    }
    View view(std::move(table_info), std::move(view_columns));
    return Result<View>::success(std::move(view));
}

} // namespace velodb
