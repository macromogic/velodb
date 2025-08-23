#pragma once

#include "catalog/column/column_base.hpp"
#include "types/value.hpp"

#include <variant>

namespace velodb {

// Forward declaration for DeviceColumn
class DeviceColumn;

class ViewColumn : public ColumnBase {
public:
    // Data source variant - compile-time dispatch for performance
    using DataSource = std::variant<const ValueVector*, // Host data (CPU memory)
                                    const DeviceColumn* // Device data (GPU memory)
                                    >;

    // View mode structures
    struct EntireView {
        // No additional data needed - views entire column
    };

    struct SliceView {
        size_t start_row;
        size_t end_row;

        SliceView(size_t start, size_t end)
            : start_row(start)
            , end_row(end)
        {
        }
    };

    struct IndicesView {
        std::vector<size_t> indices;

        explicit IndicesView(std::vector<size_t> idx)
            : indices(std::move(idx))
        {
        }
    };

    using ViewMode = std::variant<EntireView, SliceView, IndicesView>;

    // Constructors for host data (ValueVector)
    ViewColumn(DataType& type, std::string name, const ValueVector& values);
    ViewColumn(DataType& type, std::string name, const ValueVector& values, size_t start_row, size_t end_row);
    ViewColumn(DataType& type, std::string name, const ValueVector& values, std::vector<size_t> indices);

    // Constructors for device data (DeviceColumn)
    ViewColumn(DataType& type, std::string name, const DeviceColumn& device_column);
    ViewColumn(DataType& type, std::string name, const DeviceColumn& device_column, size_t start_row, size_t end_row);
    ViewColumn(DataType& type, std::string name, const DeviceColumn& device_column, std::vector<size_t> indices);

    size_t size() const override;
    const Value& get(size_t row) const override;

    DataType& getType() const override { return type_; }

    ViewColumn view() const override;
    ViewColumn viewAs(std::string alias) const override;

    // Create new views from this view
    ViewColumn slice(size_t start_row, size_t end_row) const;
    ViewColumn indices(const std::vector<size_t>& indices) const;

    std::string toString() const override;

private:
    DataType& type_;
    DataSource data_source_; // Variant holding pointer to either ValueVector or DeviceColumn
    ViewMode view_mode_;

    size_t mapToActualRow(size_t view_row) const;

    // Private constructor for internal view operations
    ViewColumn(DataType& type, std::string name, DataSource data_source, ViewMode view_mode);
};

} // namespace velodb
