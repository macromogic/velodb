#include "catalog/table_builder.hpp"

#include "catalog/column.hpp"
#include "common/exception.hpp"
#include "data/data_type.hpp"

#include <fmt/core.h>

namespace velodb {

TableBuilder::TableBuilder(std::string name, Schema schema, DataLocation location)
    : name_(std::move(name))
    , schema_(std::move(schema))
    , num_columns_(schema_.getColumnCount())
    , location_(location)
    , streaming_mode_(false)
{
    pending_columns_.resize(num_columns_);
}

void TableBuilder::enableStreamingMode(size_t estimated_rows)
{
    if (num_rows_ > 0) {
        VELODB_THROW(CatalogError, "Cannot enable streaming mode after rows have been inserted");
    }

    streaming_mode_ = true;
    pending_columns_.clear(); // Free legacy storage
    pending_columns_.shrink_to_fit();

    // Identify VARCHAR columns that need StringColumnBuilder
    for (size_t i = 0; i < num_columns_; ++i) {
        const auto& col_info = schema_.getColumnInfo(i);
        if (col_info.getType().getTypeId() == DataTypeId::VARCHAR
            || col_info.getType().getTypeId() == DataTypeId::CHAR) {
            varchar_column_indices_.push_back(i);
        }
    }

    // Create StringColumnBuilders for VARCHAR columns
    varchar_builders_.reserve(varchar_column_indices_.size());
    for (size_t i = 0; i < varchar_column_indices_.size(); ++i) {
        varchar_builders_.emplace_back(estimated_rows);
    }

    // Pre-allocate non-VARCHAR columns
    streaming_columns_.reserve(num_columns_);
    for (size_t i = 0; i < num_columns_; ++i) {
        const auto& col_info = schema_.getColumnInfo(i);
        auto type_id = col_info.getType().getTypeId();

        // Skip VARCHAR/CHAR - will be built from StringColumnBuilder at the end
        if (type_id == DataTypeId::VARCHAR || type_id == DataTypeId::CHAR) {
            // Create placeholder with 0 capacity (will be replaced in build())
            streaming_columns_.emplace_back(col_info.getType().cloneUnique(), 0, location_);
        } else {
            size_t capacity = estimated_rows > 0 ? estimated_rows : 1024;
            streaming_columns_.emplace_back(col_info.getType().cloneUnique(), capacity, location_);
            if (estimated_rows > 0) {
                streaming_columns_.back().reserve(estimated_rows);
            }
        }
    }
}

TableBuilder& TableBuilder::insertRow(const std::vector<Value>& values)
{
    validateRowSize(values);

    if (streaming_mode_) {
        // Direct column append for non-VARCHAR columns
        // Use StringColumnBuilder for VARCHAR columns (memory efficient)
        size_t varchar_idx = 0;
        for (size_t i = 0; i < num_columns_; ++i) {
            if (varchar_idx < varchar_column_indices_.size() && varchar_column_indices_[varchar_idx] == i) {
                // This is a VARCHAR column - use StringColumnBuilder
                if (values[i].isNull()) {
                    varchar_builders_[varchar_idx].appendNull();
                } else {
                    varchar_builders_[varchar_idx].append(values[i].getString());
                }
                varchar_idx++;
            } else {
                // Non-VARCHAR - append directly
                streaming_columns_[i].append(values[i]);
            }
        }
    } else {
        // Legacy mode - buffer values
        for (size_t i = 0; i < num_columns_; ++i) {
            pending_columns_[i].push_back(values[i]);
        }
    }
    num_rows_++;
    return *this;
}

TableBuilder& TableBuilder::insertRow(std::vector<Value>&& values)
{
    validateRowSize(values);

    if (streaming_mode_) {
        size_t varchar_idx = 0;
        for (size_t i = 0; i < num_columns_; ++i) {
            if (varchar_idx < varchar_column_indices_.size() && varchar_column_indices_[varchar_idx] == i) {
                if (values[i].isNull()) {
                    varchar_builders_[varchar_idx].appendNull();
                } else {
                    varchar_builders_[varchar_idx].append(values[i].getString());
                }
                varchar_idx++;
            } else {
                streaming_columns_[i].append(std::move(values[i]));
            }
        }
    } else {
        for (size_t i = 0; i < num_columns_; ++i) {
            pending_columns_[i].push_back(std::move(values[i]));
        }
    }
    num_rows_++;
    return *this;
}

TableBuilder& TableBuilder::insertRows(const std::vector<std::vector<Value>>& rows)
{
    for (const auto& row : rows) {
        insertRow(row);
    }
    return *this;
}

TableBuilder& TableBuilder::insertRows(std::vector<std::vector<Value>>&& rows)
{
    for (auto& row : rows) {
        insertRow(std::move(row));
    }
    return *this;
}

TableBuilder& TableBuilder::reserveRows(size_t row_count)
{
    if (streaming_mode_) {
        for (auto& column : streaming_columns_) {
            column.reserve(row_count);
        }
        for (auto& builder : varchar_builders_) {
            builder.reserve(row_count);
        }
    } else {
        for (auto& column : pending_columns_) {
            column.reserve(row_count);
        }
    }
    return *this;
}

Table TableBuilder::build() &&
{
    Table table(std::move(name_), std::move(schema_));

    if (streaming_mode_) {
        // Streaming mode: most columns are already built
        // VARCHAR columns need to be built from StringColumnBuilder
        for (size_t vi = 0; vi < varchar_column_indices_.size(); ++vi) {
            size_t col_idx = varchar_column_indices_[vi];

            // Build ValueVector from StringColumnBuilder
            auto vec = varchar_builders_[vi].build(location_);

            // Create Column from the built ValueVector
            auto col = Column::adopt(table.getColumnType(col_idx).cloneUnique(), std::move(vec));
            streaming_columns_[col_idx] = std::move(col);
        }
        table.columns_ = std::move(streaming_columns_);
    } else {
        // Legacy mode: build columns from pending values
        std::vector<Column> columns;
        columns.reserve(num_columns_);
        for (size_t i = 0; i < num_columns_; ++i) {
            auto column = Column::buildFrom(table.getColumnType(i).cloneUnique(),
                                            std::move(pending_columns_[i]),
                                            location_);
            columns.push_back(std::move(column));
        }
        table.columns_ = std::move(columns);
    }
    table.row_count_ = num_rows_;

    return table;
}

void TableBuilder::validateRowSize(const std::vector<Value>& row) const
{
    if (row.size() != num_columns_) {
        VELODB_THROW(CatalogError, fmt::format("Row has {} values but schema expects {}", row.size(), num_columns_));
    }
}

} // namespace velodb
