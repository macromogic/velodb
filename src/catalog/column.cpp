#include "catalog/column.hpp"
#include "common/exception.hpp"
#include <fmt/core.h>

namespace velodb {

ColumnInfo::ColumnInfo(std::string name,
    std::unique_ptr<DataType> type,
    bool is_nullable,
    bool is_unique,
    bool is_primary_key)
    : name_(std::move(name))
    , type_(std::move(type))
    , is_nullable_(is_nullable)
    , is_unique_(is_unique)
    , is_primary_key_(is_primary_key)
{
}

ColumnInfo ColumnInfo::cloneImpl() const
{
    return { name_, type_->cloneUnique(), is_nullable_, is_unique_, is_primary_key_ };
}

std::string ColumnInfo::toString() const
{
    std::string result = fmt::format("{} {}", name_, type_->toString());
    if (!is_nullable_) {
        result += " NOT NULL";
    }
    if (is_unique_) {
        result += " UNIQUE";
    }
    if (is_primary_key_) {
        result += " PRIMARY KEY";
    }
    return result;
}

Column::Column(std::string name,
    bool is_nullable,
    bool is_unique,
    bool is_primary_key)
    : name_(std::move(name))
    , is_nullable_(is_nullable)
    , is_unique_(is_unique)
    , is_primary_key_(is_primary_key)
{
}

ValueColumn::ValueColumn(std::string name,
    std::unique_ptr<DataType> type,
    bool is_nullable,
    bool is_unique,
    bool is_primary_key)
    : Column(std::move(name), is_nullable, is_unique, is_primary_key)
    , type_(std::move(type))
{
}

ValueColumn::ValueColumn(const ColumnInfo& info)
    : Column(info.getName(), info.isNullable(), info.isUnique(), info.isPrimaryKey())
    , type_(info.getType().cloneUnique())
{
}

void ValueColumn::resize(size_t new_size)
{
    values_.resize(new_size);
}

void ValueColumn::reserve(size_t new_capacity)
{
    values_.reserve(new_capacity);
}

size_t ValueColumn::size() const
{
    return values_.size();
}

const Value& ValueColumn::get(size_t row) const
{
    if (row >= values_.size()) {
        VELODB_THROW(CatalogError, "Row index out of range");
    }
    return values_[row];
}

Value& ValueColumn::operator[](size_t row)
{
    if (row >= values_.size()) {
        VELODB_THROW(CatalogError, "Row index out of range");
    }
    return values_[row];
}

void ValueColumn::append(const Value& value)
{
    if (value.getTypeId() != type_->getTypeId()) {
        VELODB_THROW(CatalogError, "Value type does not match column type");
    }
    values_.push_back(value);
}

void ValueColumn::fill(const Value& value, size_t count)
{
    values_.clear();
    values_.resize(count, value);
}

ViewColumn ValueColumn::view() const
{
    return { *type_, getName(), values_ };
}

ViewColumn ValueColumn::viewAs(std::string alias) const
{
    return { *type_, std::move(alias), values_ };
}

std::string ValueColumn::toString() const
{
    std::string result = fmt::format("ValueColumn(name={}, type={}, size={}", getName(), type_->toString(), size());
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

ViewColumn::ViewColumn(DataType& type, std::string name, const ValueVector& values)
    : Column(std::move(name), true, false, false)
    , type_(type)
    , values_(values)
    , view_mode_(EntireView {})
{
}

ViewColumn::ViewColumn(DataType& type, std::string name, const ValueVector& values,
    size_t start_row, size_t end_row)
    : Column(std::move(name), true, false, false)
    , type_(type)
    , values_(values)
    , view_mode_(SliceView { start_row, end_row })
{
    if (start_row > end_row || end_row > values.size()) {
        VELODB_THROW(CatalogError, "Invalid slice range");
    }
}

ViewColumn::ViewColumn(DataType& type, std::string name, const ValueVector& values,
    std::vector<size_t> indices)
    : Column(std::move(name), true, false, false)
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
    return std::visit([view_row](const auto& mode) -> size_t {
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
    return std::visit([this](const auto& mode) -> size_t {
        using T = std::decay_t<decltype(mode)>;

        if constexpr (std::is_same_v<T, EntireView>) {
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

const Value& ViewColumn::get(size_t row) const
{
    size_t actual_row = mapToActualRow(row);
    if (actual_row >= values_.size()) {
        VELODB_THROW(CatalogError, "Row index out of range");
    }
    return values_[actual_row];
}

ViewColumn ViewColumn::slice(size_t start_row, size_t end_row) const
{
    if (end_row > size()) {
        VELODB_THROW(CatalogError, "Slice end out of range");
    }

    return std::visit([this, start_row, end_row](const auto& mode) -> ViewColumn {
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
    return std::visit([this](const auto& mode) -> ViewColumn {
        using T = std::decay_t<decltype(mode)>;

        if constexpr (std::is_same_v<T, EntireView>) {
            return { type_, getName(), values_ };
        } else if constexpr (std::is_same_v<T, SliceView>) {
            return { type_, getName(), values_, mode.start_row, mode.end_row };
        } else if constexpr (std::is_same_v<T, IndicesView>) {
            return { type_, getName(), values_, mode.indices };
        } else {
            static_assert(std::is_same_v<T, void>, "Unhandled view mode type");
        }
    },
        view_mode_);
}

ViewColumn ViewColumn::viewAs(std::string alias) const
{
    return std::visit([this, &alias](const auto& mode) -> ViewColumn {
        using T = std::decay_t<decltype(mode)>;

        if constexpr (std::is_same_v<T, EntireView>) {
            return { type_, std::move(alias), values_ };
        } else if constexpr (std::is_same_v<T, SliceView>) {
            return { type_, std::move(alias), values_, mode.start_row, mode.end_row };
        } else if constexpr (std::is_same_v<T, IndicesView>) {
            return { type_, std::move(alias), values_, mode.indices };
        } else {
            static_assert(std::is_same_v<T, void>, "Unhandled view mode type");
        }
    },
        view_mode_);
}

std::string ViewColumn::toString() const
{
    std::string result = fmt::format("ViewColumn(name={}, type={}, size={}", getName(), type_.toString(), size());

    std::visit([&result](const auto& mode) {
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
