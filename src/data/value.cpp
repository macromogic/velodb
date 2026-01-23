#include "data/value.hpp"

#include "common/exception.hpp"
#include "data/ordinal_string.hpp"

#include <fmt/core.h>

#include <stdexcept>
#include <utility>

namespace velodb {

Value::Value(DataTypeId type_id, ValueData data)
    : type_id_(type_id)
    , data_(std::move(data))
    , is_null_(false)
{
    if (type_id == DataTypeId::BOOLEAN && !std::holds_alternative<bool>(data)) {
        VELODB_THROW(TypeError, "Type mismatch");
    }
}

Value::Value(DataTypeId type_id)
    : type_id_(type_id)
    , is_null_(true)
{
}

bool Value::getBoolean() const
{
    if (is_null_)
        VELODB_THROW(TypeError, "Cannot get value from NULL");
    if (type_id_ != DataTypeId::BOOLEAN)
        VELODB_THROW(TypeError, "Type mismatch");
    return std::get<bool>(data_);
}

int8_t Value::getTinyInt() const
{
    if (is_null_)
        VELODB_THROW(TypeError, "Cannot get value from NULL");
    if (type_id_ != DataTypeId::TINYINT)
        VELODB_THROW(TypeError, "Type mismatch");
    return std::get<int8_t>(data_);
}

int16_t Value::getSmallInt() const
{
    if (is_null_)
        VELODB_THROW(TypeError, "Cannot get value from NULL");
    if (type_id_ != DataTypeId::SMALLINT)
        VELODB_THROW(TypeError, "Type mismatch");
    return std::get<int16_t>(data_);
}

int32_t Value::getInteger() const
{
    if (is_null_)
        VELODB_THROW(TypeError, "Cannot get value from NULL");
    if (type_id_ != DataTypeId::INTEGER)
        VELODB_THROW(TypeError, "Type mismatch");
    return std::get<int32_t>(data_);
}

int64_t Value::getBigInt() const
{
    if (is_null_)
        VELODB_THROW(TypeError, "Cannot get value from NULL");
    if (type_id_ != DataTypeId::BIGINT)
        VELODB_THROW(TypeError, "Type mismatch");
    return std::get<int64_t>(data_);
}

float Value::getFloat() const
{
    if (is_null_)
        VELODB_THROW(TypeError, "Cannot get value from NULL");
    if (type_id_ != DataTypeId::FLOAT)
        VELODB_THROW(TypeError, "Type mismatch");
    return std::get<float>(data_);
}

double Value::getDouble() const
{
    if (is_null_)
        VELODB_THROW(TypeError, "Cannot get value from NULL");
    if (type_id_ != DataTypeId::DOUBLE)
        VELODB_THROW(TypeError, "Type mismatch");
    return std::get<double>(data_);
}

std::string Value::getString() const
{
    if (is_null_)
        VELODB_THROW(TypeError, "Cannot get value from NULL");
    if (type_id_ != DataTypeId::VARCHAR)
        VELODB_THROW(TypeError, "Type mismatch");
    return std::get<OrdinalString>(data_);
}

bool Value::operator==(const Value& other) const
{
    if (is_null_ && other.is_null_)
        return true;
    if (is_null_ || other.is_null_)
        return false;
    if (type_id_ != other.type_id_)
        return false;
    return data_ == other.data_;
}

bool Value::operator!=(const Value& other) const
{
    return !(*this == other);
}

bool Value::operator<(const Value& other) const
{
    if (is_null_)
        return !other.is_null_; // NULL is less than any non-NULL value
    else if (other.is_null_)
        return false; // Non-NULL is not less than NULL
    if (type_id_ != other.type_id_)
        return false;

    switch (type_id_) {
    case DataTypeId::BOOLEAN:
        return !getBoolean() && other.getBoolean(); // false < true
    case DataTypeId::INTEGER:
        return getInteger() < other.getInteger();
    case DataTypeId::BIGINT:
        return getBigInt() < other.getBigInt();
    case DataTypeId::DOUBLE:
        return getDouble() < other.getDouble();
    case DataTypeId::VARCHAR:
        return getString() < other.getString();
    default:
        return false;
    }
}

bool Value::operator<=(const Value& other) const
{
    return *this < other || *this == other;
}

bool Value::operator>(const Value& other) const
{
    return !(*this <= other);
}

bool Value::operator>=(const Value& other) const
{
    return !(*this < other);
}

std::string Value::toString() const
{
    if (is_null_)
        return "NULL";

    switch (type_id_) {
    case DataTypeId::BOOLEAN:
        return getBoolean() ? "true" : "false";
    case DataTypeId::INTEGER:
        return fmt::format("{}", getInteger());
    case DataTypeId::BIGINT:
        return fmt::format("{}", getBigInt());
    case DataTypeId::DOUBLE:
        return fmt::format("{}", getDouble());
    case DataTypeId::VARCHAR:
        return fmt::format("'{}'", getString());
        // {
        //     auto ordinal_str = std::get<OrdinalString>(data_);
        //     return ordinal_str.toString();
        // }
    default:
        return "UNKNOWN";
    }
}

// Template specializations for universal get method
template <>
bool Value::get<bool>() const
{
    return getBoolean();
}

template <>
uint8_t Value::get<uint8_t>() const
{
    return getBoolean() ? 1 : 0;
}

template <>
int8_t Value::get<int8_t>() const
{
    return getTinyInt();
}

template <>
int16_t Value::get<int16_t>() const
{
    return getSmallInt();
}

template <>
int32_t Value::get<int32_t>() const
{
    return getInteger();
}

template <>
int64_t Value::get<int64_t>() const
{
    return getBigInt();
}

template <>
float Value::get<float>() const
{
    return getFloat();
}

template <>
double Value::get<double>() const
{
    return getDouble();
}

template <>
std::string Value::get<std::string>() const
{
    return getString();
}

template <>
size_t Value::get<size_t>() const
{
    if (is_null_)
        VELODB_THROW(TypeError, "Cannot get value from NULL");
    if (type_id_ != DataTypeId::VARCHAR)
        VELODB_THROW(TypeError, "Type mismatch");
    return std::get<OrdinalString>(data_).getOrdinal();
}

template <>
OrdinalString Value::get<OrdinalString>() const
{
    if (is_null_)
        VELODB_THROW(TypeError, "Cannot get value from NULL");
    if (type_id_ != DataTypeId::VARCHAR)
        VELODB_THROW(TypeError, "Type mismatch");
    return std::get<OrdinalString>(data_);
}

template <>
uint32_t Value::get<uint32_t>() const
{
    if (is_null_)
        VELODB_THROW(TypeError, "Cannot get value from NULL");
    if (type_id_ != DataTypeId::DATE)
        VELODB_THROW(TypeError, "Type mismatch");
    return std::get<uint32_t>(data_);
}

ValueData& Value::getData()
{
    if (is_null_)
        VELODB_THROW(TypeError, "Cannot get data from NULL value");
    return data_;
}

Value Value::createBoolean(bool value)
{
    return { DataTypeId::BOOLEAN, value };
}

Value Value::createTinyInt(int8_t value)
{
    return { DataTypeId::TINYINT, value };
}

Value Value::createSmallInt(int16_t value)
{
    return { DataTypeId::SMALLINT, value };
}

Value Value::createInteger(int32_t value)
{
    return { DataTypeId::INTEGER, value };
}

Value Value::createBigInt(int64_t value)
{
    return { DataTypeId::BIGINT, value };
}

Value Value::createFloat(float value)
{
    return { DataTypeId::FLOAT, value };
}

Value Value::createDouble(double value)
{
    return { DataTypeId::DOUBLE, value };
}

Value Value::createString(const std::string& value)
{
    return { DataTypeId::VARCHAR, OrdinalString(value) };
}

Value Value::createDate(const std::string& value)
{
    // convert value to uint32_t representing the date (YYYYMMDD in decimal)
    uint32_t date_value = 0;
    for (auto& ch : value) {
        if (ch != '-') {
            date_value = date_value * 10 + (ch - '0');
        }
    }
    return { DataTypeId::DATE, date_value };
}

Value Value::createNull(DataTypeId type_id)
{
    return Value { type_id };
}

} // namespace velodb
