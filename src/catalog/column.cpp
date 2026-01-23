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
        return ValueVector<VT>(initial_capacity, location);                                                            \
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
}

Column Column::buildFrom(std::unique_ptr<DataType> type, std::vector<Value>&& values, DataLocation location)
{
    PROFILE_SCOPE("Column::buildFrom");
    switch (type->getTypeId()) {
#define X(name, DT, VT)                                                                                                \
    case DataTypeId::name: {                                                                                           \
        auto vec = ValueVector<VT>::buildFrom(std::move(values), location);                                            \
        return Column(type->cloneUnique(), std::move(vec));                                                            \
    }
        LIST_TYPES(X)
#undef X
    case DataTypeId::CHAR: {
        auto vec = ValueVector<OrdinalString>::buildFrom(std::move(values), location);
        return Column(type->cloneUnique(), std::move(vec));
    }
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
    return std::visit([](auto&& vv) { return vv.size(); }, data_source_);
}

Value Column::get(size_t index) const
{
    return std::visit([index](auto&& vv) { return vv.get(index); }, data_source_);
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

void* Column::rawData()
{
    return std::visit([](auto&& vv) { return static_cast<void*>(vv.data()); }, data_source_);
}

const void* Column::rawData() const
{
    return std::visit([](auto&& vv) { return static_cast<const void*>(vv.data()); }, data_source_);
}

BitVector::Element* Column::rawBitmapData()
{
    return std::visit([](auto&& vv) { return vv.null_mask_.data(); }, data_source_);
}

const BitVector::Element* Column::rawBitmapData() const
{
    return std::visit([](auto&& vv) { return vv.null_mask_.data(); }, data_source_);
}

void* Column::getDeviceBuffer() const
{
    return std::visit(
        [](auto&& vv) {
            using DType = typename std::decay_t<decltype(vv)>::DType;
            VELODB_ASSERT_MSG(vv.location_ == DataLocation::CUDA,
                              "Cannot get temporary buffer for non-device location");
            DType* ptr;
            auto stream_handle = StreamPool::getInstance().acquire().value();
            CHECKED_CALL_THROW(cudaMallocAsync(&ptr, vv.capacity_ * sizeof(DType), stream_handle->get()));
            return static_cast<void*>(ptr);
        },
        data_source_);
}

BitVector::Element* Column::getDeviceBitmapBuffer() const
{
    return std::visit(
        [](auto&& vv) {
            VELODB_ASSERT_MSG(vv.location_ == DataLocation::CUDA,
                              "Cannot get temporary bitmap buffer for non-device location");
            BitVector::Element* ptr;
            auto stream_handle = StreamPool::getInstance().acquire().value();
            CHECKED_CALL_THROW(cudaMallocAsync(&ptr,
                                               vv.null_mask_.element_capacity_ * sizeof(BitVector::Element),
                                               stream_handle->get()));
            CHECKED_CALL_THROW(cudaMemsetAsync(ptr,
                                               0,
                                               vv.null_mask_.element_capacity_ * sizeof(BitVector::Element),
                                               stream_handle->get()));
            return ptr;
        },
        data_source_);
}

void Column::setFromDeviceBuffers(void* data, BitVector::Element* bitmap_data)
{
    std::visit(
        [&data, &bitmap_data](auto&& vv) {
            using DType = typename std::decay_t<decltype(vv)>::DType;
            VELODB_ASSERT_MSG(vv.location_ == DataLocation::CUDA, "Cannot set data for non-device location");
            auto stream_handle = StreamPool::getInstance().acquire().value();
            if (data) {
                DType* old_data = vv.data_;
                vv.data_ = static_cast<DType*>(data);
                CHECKED_CALL_THROW(cudaFreeAsync(old_data, stream_handle->get()));
            }
            if (bitmap_data) {
                BitVector::Element* old_bitmap = vv.null_mask_.data_;
                vv.null_mask_.data_ = bitmap_data;
                CHECKED_CALL_THROW(cudaFreeAsync(old_bitmap, stream_handle->get()));
            }
        },
        data_source_);
}

DataLocation Column::location() const
{
    return std::visit([](auto&& vv) { return vv.location(); }, data_source_);
}

void Column::to(DataLocation location)
{
    std::visit([location](auto&& vv) { vv.to(location); }, data_source_);
}

void Column::reserve(size_t new_capacity)
{
    std::visit([new_capacity](auto&& vv) { vv.reserve(new_capacity); }, data_source_);
}

void Column::append(const Value& value)
{
    std::visit(
        [&](auto&& vv) {
            using ImplType = std::decay_t<decltype(vv)>;
            using DType = typename ImplType::DType;

            if (value.isNull()) {
                // Use default-constructed value for null values
                vv.append(DType {});
            } else {
                vv.append(value.get<DType>());
            }
        },
        data_source_);
}

void Column::append(Value&& value)
{
    std::visit(
        [&](auto&& vv) {
            using ImplType = std::decay_t<decltype(vv)>;
            using DType = typename ImplType::DType;

            if (value.isNull()) {
                // Use default-constructed value for null values
                vv.append(DType {});
            } else {
                vv.append(std::move(value.get<DType>()));
            }
        },
        data_source_);
}

Column Column::slice(size_t start, size_t end) const
{
    VELODB_ASSERT_MSG(start <= end && end <= size(), "Invalid slice range");
    return std::visit(
        [start, end, this](auto&& vv) -> Column {
            auto sliced = vv.slice(start, end);
            return Column(type_->cloneUnique(), std::move(sliced));
        },
        data_source_);
}

Column Column::gather(const Column& rowids) const
{
    return std::visit(
        [this](auto&& src_vv, auto&& rowid_vv) -> Column {
            using RowidType = std::decay_t<decltype(rowid_vv)>;
            if constexpr (std::is_same_v<typename RowidType::VType, int64_t>) {
                auto vec = src_vv.gather(rowid_vv);
                return Column(type_->cloneUnique(), std::move(vec));
            } else {
                VELODB_THROW(ExecutionError, "Rowid column must be of type BIGINT for gather");
            }
        },
        data_source_,
        rowids.data_source_);
}

Column Column::tryOwn()
{
    return std::visit([this](auto&& vv) -> Column { return Column(type_->cloneUnique(), vv.tryOwn()); }, data_source_);
}

void Column::setSize(size_t new_size)
{
    std::visit([new_size](auto&& vv) { vv.setSize(new_size); }, data_source_);
}

void Column::debug() const
{
    std::visit(
        [](auto&& vv) {
            fmt::print("Column debug (size={}, capacity={}, location={}): ", vv.size(), vv.capacity(), vv.location());
            for (size_t i = 0; i < vv.size(); ++i) {
                fmt::print("{} ", vv.get(i));
            }
            fmt::println("");
        },
        data_source_);
}

void Column::debug(size_t max_elements) const
{
    std::visit(
        [max_elements](auto&& vv) {
            fmt::print("Column debug (size={}, capacity={}, location={}): ", vv.size(), vv.capacity(), vv.location());
            size_t limit = std::min(vv.size(), max_elements);
            for (size_t i = 0; i < limit; ++i) {
                fmt::print("{} ", vv.get(i));
            }
            if (vv.size() > max_elements) {
                fmt::print("... ({} more elements)", vv.size() - max_elements);
            }
            fmt::println("");
        },
        data_source_);
}

} // namespace velodb
