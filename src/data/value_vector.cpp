#include "data/value_vector.hpp"

#include "common/exception.hpp"
#include "data/data_type.hpp"
#include "data/value.hpp"

namespace velodb {

namespace {
    // Helper function to create the appropriate ValueVectorImpl based on data type
    ValueVector::DataSource createDataSource(const DataType& type, size_t initial_capacity)
    {
        switch (type.getTypeId()) {
#define X(name, DT)                                                                                                    \
    case DataTypeId::name: {                                                                                           \
        if constexpr (std::is_same_v<DT, FixedString>) {                                                               \
            return ValueVectorImpl<DT>(initial_capacity, type.size());                                                 \
        } else {                                                                                                       \
            return ValueVectorImpl<DT>(initial_capacity);                                                              \
        }                                                                                                              \
    }
            LIST_DTYPES(X)
#undef X
        default:
            VELODB_THROW(ExecutionError, "Unsupported data type");
        }
    }
} // anonymous namespace

ValueVector::ValueVector(std::unique_ptr<DataType> type, size_t initial_capacity)
    : type_(std::move(type))
    , data_source_(createDataSource(*type_, initial_capacity))
{
}

size_t ValueVector::size() const
{
    return std::visit([](auto&& arg) { return arg.size(); }, data_source_);
}

Value ValueVector::get(size_t index) const
{
    return std::visit([index](auto&& arg) { return arg.get(index); }, data_source_);
}

Value ValueVector::operator[](size_t index) const
{
    return get(index);
}

DataLocation ValueVector::location() const
{
    return std::visit([](auto&& arg) { return arg.location(); }, data_source_);
}

void ValueVector::reserve(size_t new_capacity)
{
    std::visit([new_capacity](auto&& arg) { arg.reserve(new_capacity); }, data_source_);
}

void ValueVector::resize(size_t new_size, const Value& value)
{
    std::visit(
        [new_size, &value](auto&& arg) {
            using ImplType = std::decay_t<decltype(arg)>;
            using DType = typename ImplType::DType;

            if (value.isNull()) {
                // Use default-constructed value for null values
                arg.resize(new_size, DType {});
            } else {
                arg.resize(new_size, value.get<DType>());
            }
        },
        data_source_);
}

void ValueVector::clear()
{
    std::visit([](auto&& arg) { arg.clear(); }, data_source_);
}

void ValueVector::append(const Value& value)
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

void ValueVector::append(Value&& value)
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

} // namespace velodb
