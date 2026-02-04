#pragma once

#include "catalog/column.hpp"
#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include "common/copy_traits.hpp"
#include "data/data_location.hpp"
#include "data/string_column_builder.hpp"
#include "data/value.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace velodb {

class TableBuilder : private NonCopyable {
public:
    TableBuilder(std::string name, Schema schema, DataLocation location = DataLocation::HOST);
    TableBuilder(TableBuilder&& other) noexcept = default;
    TableBuilder& operator=(TableBuilder&& other) noexcept = default;

    ~TableBuilder() = default;

    TableBuilder& insertRow(const std::vector<Value>& values);
    TableBuilder& insertRow(std::vector<Value>&& values);
    TableBuilder& insertRows(const std::vector<std::vector<Value>>& rows);
    TableBuilder& insertRows(std::vector<std::vector<Value>>&& rows);
    TableBuilder& reserveRows(size_t row_count);

    // Inspection methods
    const std::string& getName() const { return name_; }

    Table build() &&;

    void enableStreamingMode(size_t estimated_rows = 0);
    bool isStreamingMode() const { return streaming_mode_; }

private:
    std::string name_;
    Schema schema_;
    size_t num_columns_;
    size_t num_rows_ = 0;
    DataLocation location_;

    // Legacy mode: buffer all values (high memory usage)
    std::vector<std::vector<Value>> pending_columns_;

    // Streaming mode: write directly to columns (low memory usage)
    bool streaming_mode_ = false;
    std::vector<Column> streaming_columns_;

    // In streaming mode, VARCHAR columns use StringColumnBuilder for memory efficiency
    // StringColumnBuilder uses ~5% of the memory compared to Value-based buffering
    std::vector<size_t> varchar_column_indices_;
    std::vector<StringColumnBuilder> varchar_builders_;

    void validateRowSize(const std::vector<Value>& row) const;
};

} // namespace velodb
