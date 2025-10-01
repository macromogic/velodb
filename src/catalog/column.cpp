#include "catalog/column.hpp"

#include "common/exception.hpp"
#include "common/profiler.hpp"

#include <fmt/format.h>

#include <type_traits>

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
    std::string result = fmt::format("{} {}", name_, *type_);
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

// Helper function to create the appropriate ValueVector based on data type
static Column::DataSource createDataSource(const DataType& type, size_t initial_capacity, DataLocation location)
{
    PROFILE_SCOPE("createDataSource");

    switch (type.getTypeId()) {
#define X(name, DT, VT)                                                                                                \
    case DataTypeId::name: {                                                                                           \
        PROFILE_SCOPE("Creating ValueVector for " #name);                                                              \
        return ValueVector<DT>(initial_capacity, location);                                                            \
    }
        LIST_TYPES(X)
#undef X
    default:
        VELODB_THROW(ExecutionError, "Unsupported data type");
    }
}

Column::Column(std::unique_ptr<DataType> type, size_t initial_capacity, DataLocation location)
    : type_(std::move(type))
    , data_source_(createDataSource(*type_, initial_capacity, location))
{
    PROFILE_SCOPE("Column Constructor");
}

Column Column::buildFrom(std::unique_ptr<DataType> type, std::vector<Value>&& values, DataLocation location)
{
    PROFILE_SCOPE("Column::buildFrom");
    switch (type->getTypeId()) {
#define X(name, DT, VT)                                                                                                \
    case DataTypeId::name: {                                                                                           \
        auto vec = ValueVector<DT>::buildFrom(std::move(values), location);                                            \
        return Column(type->cloneUnique(), std::move(vec));                                                            \
    }
        LIST_TYPES(X)
#undef X
    default:
        VELODB_THROW(ExecutionError, "Unsupported data type");
    }
}

const DataType& Column::getType() const
{
    return *type_;
}

size_t Column::size() const
{
    return std::visit([](auto&& arg) { return arg.size(); }, data_source_);
}

Value Column::get(size_t index) const
{
    return std::visit([index](auto&& arg) { return arg.get(index); }, data_source_);
}

Value Column::operator[](size_t index) const
{
    return get(index);
}

void Column::ensureOrdinal(Value& value, ComparisonType comp) const
{
    std::visit(
        [&value, comp](auto&& vec) {
            using VecVType = typename std::decay_t<decltype(vec)>::VType;
            if constexpr (std::is_same_v<VecVType, OrdinalString>) {
                if (!value.isNull() && value.getTypeId() != DataTypeId::VARCHAR) {
                    VELODB_THROW(TypeError, "Type mismatch");
                }
                auto& ordinal_string = std::get<OrdinalString>(value.getData());
                vec.ensureOrdinal(ordinal_string, comp);
            } else {
                // For non-string types, no action needed
                (void)vec; // Suppress unused variable warning
            }
        },
        data_source_);
}

DataLocation Column::location() const
{
    return std::visit([](auto&& arg) { return arg.location(); }, data_source_);
}

void Column::to(DataLocation location)
{
    std::visit([location](auto&& arg) { arg.to(location); }, data_source_);
}

void Column::reserve(size_t new_capacity)
{
    std::visit([new_capacity](auto&& arg) { arg.reserve(new_capacity); }, data_source_);
}

void Column::append(const Value& value)
{
    std::visit(
        [&](auto&& arg) {
            using ImplType = std::decay_t<decltype(arg)>;
            using DType = typename ImplType::DType;

            if (value.isNull()) {
                // Use default-constructed value for null values
                arg.append(DType {});
            } else {
                arg.append(value.get<DType>());
            }
        },
        data_source_);
}

void Column::append(Value&& value)
{
    std::visit(
        [&](auto&& arg) {
            using ImplType = std::decay_t<decltype(arg)>;
            using DType = typename ImplType::DType;

            if (value.isNull()) {
                // Use default-constructed value for null values
                arg.append(DType {});
            } else {
                arg.append(std::move(value.get<DType>()));
            }
        },
        data_source_);
}

Column Column::slice(size_t start, size_t end) const
{
    VELODB_ASSERT_MSG(start <= end && end <= size(), "Invalid slice range");
    return std::visit(
        [start, end, this](auto&& arg) -> Column {
            auto sliced_impl = arg.slice(start, end);
            return Column(type_->cloneUnique(), std::move(sliced_impl));
        },
        data_source_);
}

Column Column::tryOwn()
{
    return std::visit([this](auto&& arg) -> Column { return Column(type_->cloneUnique(), arg.tryOwn()); },
                      data_source_);
}

Column Column::splitFront(size_t size)
{
    return std::visit(
        [this, size](auto&& arg) -> Column {
            auto split_impl = arg.splitFront(size);
            return Column(type_->cloneUnique(), std::move(split_impl));
        },
        data_source_);
}

void Column::reorder(const Column& rowid_column)
{
    std::visit(
        [&](auto&& data, auto&& idx) {
            using IdxType = typename std::decay_t<decltype(idx)>::DType;
            if constexpr (std::is_same_v<IdxType, int64_t>) {
                data.reorder(idx);
            } else {
                VELODB_THROW(ExecutionError, "Invalid types for reordering");
            }
        },
        data_source_,
        rowid_column.data_source_);
}

void Column::appendMultiple(const Column& other, const Column& mask)
{
    std::visit(
        [](auto&& dst, auto&& src, auto&& mask) {
            using DstType = std::decay_t<decltype(dst)>;
            using SrcType = std::decay_t<decltype(src)>;
            using MaskType = std::decay_t<decltype(mask)>;
            if constexpr (std::is_same_v<DstType, SrcType> && std::is_same_v<typename MaskType::VType, bool>) {
                dst.appendMultiple(src, mask);
            } else {
                VELODB_THROW(ExecutionError, "Invalid types for compaction");
            }
        },
        data_source_,
        other.data_source_,
        mask.data_source_);
}

void Column::appendMultiple(const Column& other)
{
    std::visit(
        [](auto&& dst, auto&& src) {
            using DstType = std::decay_t<decltype(dst)>;
            using SrcType = std::decay_t<decltype(src)>;
            if constexpr (std::is_same_v<DstType, SrcType>) {
                dst.appendMultiple(src);
            } else {
                VELODB_THROW(ExecutionError, "Invalid types for append");
            }
        },
        data_source_,
        other.data_source_);
}

} // namespace velodb
