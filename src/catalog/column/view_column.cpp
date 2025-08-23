#include "catalog/column/view_column.hpp"

#include "catalog/column/column_base.hpp"
#include "catalog/column/device_column.hpp"
#include "common/exception.hpp"
#include "common/fmt.hpp"

namespace velodb {

// Host data constructors
ViewColumn::ViewColumn(DataType& type, std::string name, const ValueVector& values)
    : ColumnBase(std::move(name), true, false, false)
    , type_(type)
    , data_source_(&values)
    , view_mode_(EntireView {})
{
}

ViewColumn::ViewColumn(DataType& type, std::string name, const ValueVector& values, size_t start_row, size_t end_row)
    : ColumnBase(std::move(name), true, false, false)
    , type_(type)
    , data_source_(&values)
    , view_mode_(SliceView { start_row, end_row })
{
    if (start_row > end_row || end_row > values.size()) {
        VELODB_THROW(CatalogError, "Invalid slice range");
    }
}

ViewColumn::ViewColumn(DataType& type, std::string name, const ValueVector& values, std::vector<size_t> indices)
    : ColumnBase(std::move(name), true, false, false)
    , type_(type)
    , data_source_(&values)
    , view_mode_(IndicesView { std::move(indices) })
{
    const auto& idx_view = std::get<IndicesView>(view_mode_);
    for (size_t idx : idx_view.indices) {
        if (idx >= values.size()) {
            VELODB_THROW(CatalogError, "Index out of range in discrete view");
        }
    }
}

// Device data constructors
ViewColumn::ViewColumn(DataType& type, std::string name, const DeviceColumn& device_column)
    : ColumnBase(std::move(name), true, false, false)
    , type_(type)
    , data_source_(&device_column)
    , view_mode_(EntireView {})
{
}

ViewColumn::ViewColumn(DataType& type,
                       std::string name,
                       const DeviceColumn& device_column,
                       size_t start_row,
                       size_t end_row)
    : ColumnBase(std::move(name), true, false, false)
    , type_(type)
    , data_source_(&device_column)
    , view_mode_(SliceView { start_row, end_row })
{
    if (start_row > end_row || end_row > device_column.size()) {
        VELODB_THROW(CatalogError, "Invalid slice range");
    }
}

ViewColumn::ViewColumn(DataType& type, std::string name, const DeviceColumn& device_column, std::vector<size_t> indices)
    : ColumnBase(std::move(name), true, false, false)
    , type_(type)
    , data_source_(&device_column)
    , view_mode_(IndicesView { std::move(indices) })
{
    const auto& idx_view = std::get<IndicesView>(view_mode_);
    for (size_t idx : idx_view.indices) {
        if (idx >= device_column.size()) {
            VELODB_THROW(CatalogError, "Index out of range in discrete view");
        }
    }
}

// Private constructor for internal operations
ViewColumn::ViewColumn(DataType& type, std::string name, DataSource data_source, ViewMode view_mode)
    : ColumnBase(std::move(name), true, false, false)
    , type_(type)
    , data_source_(data_source)
    , view_mode_(std::move(view_mode))
{
}

size_t ViewColumn::mapToActualRow(size_t view_row) const
{
    return std::visit(
        [view_row](const auto& mode) -> size_t {
            using T = std::decay_t<decltype(mode)>;

            if constexpr (std::is_same_v<T, EntireView>) {
                return view_row;
            } else if constexpr (std::is_same_v<T, SliceView>) {
                return mode.start_row + view_row;
            } else if constexpr (std::is_same_v<T, IndicesView>) {
                if (view_row >= mode.indices.size()) {
                    VELODB_THROW(CatalogError, "View row index out of range");
                }
                return mode.indices[view_row];
            } else {
                static_assert(std::is_same_v<T, void>, "Unhandled view mode type");
            }
        },
        view_mode_);
}

size_t ViewColumn::size() const
{
    return std::visit(
        [this](const auto& mode) -> size_t {
            using T = std::decay_t<decltype(mode)>;

            if constexpr (std::is_same_v<T, EntireView>) {
                // Get size from data source
                return std::visit([](const auto& data_source) -> size_t { return data_source->size(); }, data_source_);
            } else if constexpr (std::is_same_v<T, SliceView>) {
                return mode.end_row - mode.start_row;
            } else if constexpr (std::is_same_v<T, IndicesView>) {
                return mode.indices.size();
            } else {
                static_assert(std::is_same_v<T, void>, "Unhandled view mode type");
            }
        },
        view_mode_);
}

const Value& ViewColumn::get(size_t row) const
{
    size_t actual_row = mapToActualRow(row);

    // Use variant to dispatch to appropriate data source
    return std::visit(
        [actual_row](const auto& data_source) -> const Value& {
            using DataSourceType = std::decay_t<decltype(*data_source)>;

            if constexpr (std::is_same_v<DataSourceType, ValueVector>) {
                // Host data access - direct memory access
                if (actual_row >= data_source->size()) {
                    VELODB_THROW(CatalogError, "Row index out of range");
                }
                return (*data_source)[actual_row];
            } else if constexpr (std::is_same_v<DataSourceType, DeviceColumn>) {
                VELODB_THROW(CatalogError, "Cannot access device data");
            } else {
                static_assert(std::is_same_v<DataSourceType, void>, "Unhandled data source type");
            }
        },
        data_source_);
}

ViewColumn ViewColumn::slice(size_t start_row, size_t end_row) const
{
    if (end_row > size()) {
        VELODB_THROW(CatalogError, "Slice end out of range");
    }

    return std::visit(
        [this, start_row, end_row](const auto& mode) -> ViewColumn {
            using T = std::decay_t<decltype(mode)>;

            if constexpr (std::is_same_v<T, EntireView> || std::is_same_v<T, SliceView>) {
                // For entire and slice views, we can create a new slice directly
                size_t actual_start = mapToActualRow(start_row);
                size_t actual_end = mapToActualRow(end_row - 1) + 1;
                return ViewColumn(type_, getName(), data_source_, SliceView { actual_start, actual_end });
            } else if constexpr (std::is_same_v<T, IndicesView>) {
                // For indices view, create a new indices vector from the slice
                std::vector<size_t> new_indices;
                new_indices.reserve(end_row - start_row);
                for (size_t i = start_row; i < end_row; ++i) {
                    new_indices.push_back(mode.indices[i]);
                }
                return ViewColumn(type_, getName(), data_source_, IndicesView { std::move(new_indices) });
            } else {
                static_assert(std::is_same_v<T, void>, "Unhandled view mode type");
            }
        },
        view_mode_);
}

ViewColumn ViewColumn::indices(const std::vector<size_t>& indices) const
{
    std::vector<size_t> actual_indices;
    actual_indices.reserve(indices.size());

    for (size_t view_idx : indices) {
        if (view_idx >= size()) {
            VELODB_THROW(CatalogError, "Index out of range for view");
        }
        actual_indices.push_back(mapToActualRow(view_idx));
    }

    return ViewColumn(type_, getName(), data_source_, IndicesView { std::move(actual_indices) });
}

ViewColumn ViewColumn::view() const
{
    return std::visit(
        [this](const auto& mode) -> ViewColumn {
            using T = std::decay_t<decltype(mode)>;

            if constexpr (std::is_same_v<T, EntireView>) {
                return ViewColumn(type_, getName(), data_source_, EntireView {});
            } else if constexpr (std::is_same_v<T, SliceView>) {
                return ViewColumn(type_, getName(), data_source_, SliceView { mode.start_row, mode.end_row });
            } else if constexpr (std::is_same_v<T, IndicesView>) {
                return ViewColumn(type_, getName(), data_source_, IndicesView { mode.indices });
            } else {
                static_assert(std::is_same_v<T, void>, "Unhandled view mode type");
            }
        },
        view_mode_);
}

ViewColumn ViewColumn::viewAs(std::string alias) const
{
    return std::visit(
        [this, &alias](const auto& mode) -> ViewColumn {
            using T = std::decay_t<decltype(mode)>;

            if constexpr (std::is_same_v<T, EntireView>) {
                return ViewColumn(type_, std::move(alias), data_source_, EntireView {});
            } else if constexpr (std::is_same_v<T, SliceView>) {
                return ViewColumn(type_, std::move(alias), data_source_, SliceView { mode.start_row, mode.end_row });
            } else if constexpr (std::is_same_v<T, IndicesView>) {
                return ViewColumn(type_, std::move(alias), data_source_, IndicesView { mode.indices });
            } else {
                static_assert(std::is_same_v<T, void>, "Unhandled view mode type");
            }
        },
        view_mode_);
}

std::string ViewColumn::toString() const
{
    std::string result = fmt::format("ViewColumn(name={}, type={}, size={}", getName(), type_, size());

    // Add data source type information
    std::visit(
        [&result](const auto& data_source) {
            using DataSourceType = std::decay_t<decltype(*data_source)>;
            if constexpr (std::is_same_v<DataSourceType, ValueVector>) {
                result += ", source=HOST";
            } else if constexpr (std::is_same_v<DataSourceType, DeviceColumn>) {
                result += ", source=DEVICE";
            }
        },
        data_source_);

    // Add view mode information
    std::visit(
        [&result](const auto& mode) {
            using T = std::decay_t<decltype(mode)>;

            if constexpr (std::is_same_v<T, EntireView>) {
                result += ", view=ENTIRE";
            } else if constexpr (std::is_same_v<T, SliceView>) {
                result += fmt::format(", view=SLICE[{}:{})", mode.start_row, mode.end_row);
            } else if constexpr (std::is_same_v<T, IndicesView>) {
                result += fmt::format(", view=INDICES[{} indices]", mode.indices.size());
            } else {
                static_assert(std::is_same_v<T, void>, "Unhandled view mode type");
            }
        },
        view_mode_);

    if (!isNullable()) {
        result += ", not null";
    }
    if (isUnique()) {
        result += ", unique";
    }
    if (isPrimaryKey()) {
        result += ", primary key";
    }
    result += ")";
    return result;
}

} // namespace velodb
