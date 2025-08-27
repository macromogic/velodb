#include "catalog/column/view_column.hpp"

#include "catalog/column/column_base.hpp"
#include "common/exception.hpp"
#include "common/fmt.hpp"
#include "data/value_vector.hpp"

namespace velodb {

// Host data constructors
ViewColumn::ViewColumn(DataType& type, std::string name, const ValueVector& values)
    : ColumnBase(std::move(name), true, false, false)
    , type_(type)
    , values_(values)
    , view_mode_(EntireView {})
{
}

ViewColumn::ViewColumn(DataType& type, std::string name, const ValueVector& values, size_t start_row, size_t end_row)
    : ColumnBase(std::move(name), true, false, false)
    , type_(type)
    , values_(values)
    , view_mode_(SliceView { start_row, end_row })
{
    if (start_row > end_row || end_row > values.size()) {
        VELODB_THROW(CatalogError, "Invalid slice range");
    }
}

ViewColumn::ViewColumn(DataType& type, std::string name, const ValueVector& values, std::vector<size_t> indices)
    : ColumnBase(std::move(name), true, false, false)
    , type_(type)
    , values_(values)
    , view_mode_(IndicesView { std::move(indices) })
{
    const auto& idx_view = std::get<IndicesView>(view_mode_);
    for (size_t idx : idx_view.indices) {
        if (idx >= values.size()) {
            VELODB_THROW(CatalogError, "Index out of range in discrete view");
        }
    }
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
                return values_.size();
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

Value ViewColumn::get(size_t row) const
{
    size_t actual_row = mapToActualRow(row);
    return values_.get(actual_row);
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
                return ViewColumn(type_, getName(), values_, actual_start, actual_end);
            } else if constexpr (std::is_same_v<T, IndicesView>) {
                // For indices view, create a new indices vector from the slice
                std::vector<size_t> new_indices;
                new_indices.reserve(end_row - start_row);
                for (size_t i = start_row; i < end_row; ++i) {
                    new_indices.push_back(mode.indices[i]);
                }
                return ViewColumn(type_, getName(), values_, std::move(new_indices));
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

    return ViewColumn(type_, getName(), values_, std::move(actual_indices));
}

ViewColumn ViewColumn::view() const
{
    return std::visit(
        [this](const auto& mode) -> ViewColumn {
            using T = std::decay_t<decltype(mode)>;

            if constexpr (std::is_same_v<T, EntireView>) {
                return ViewColumn(type_, getName(), values_);
            } else if constexpr (std::is_same_v<T, SliceView>) {
                return ViewColumn(type_, getName(), values_, mode.start_row, mode.end_row);
            } else if constexpr (std::is_same_v<T, IndicesView>) {
                return ViewColumn(type_, getName(), values_, mode.indices);
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
                return ViewColumn(type_, std::move(alias), values_);
            } else if constexpr (std::is_same_v<T, SliceView>) {
                return ViewColumn(type_, std::move(alias), values_, mode.start_row, mode.end_row);
            } else if constexpr (std::is_same_v<T, IndicesView>) {
                return ViewColumn(type_, std::move(alias), values_, mode.indices);
            } else {
                static_assert(std::is_same_v<T, void>, "Unhandled view mode type");
            }
        },
        view_mode_);
}

std::string ViewColumn::toString() const
{
    std::string result = fmt::format("ViewColumn(name={}, type={}, size={}, location={}",
                                     getName(),
                                     type_,
                                     size(),
                                     values_.location());

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
