#include "types/value.hpp"

#include "common/exception.hpp"

#include <fmt/core.h>

#include <stdexcept>
#include <utility>

namespace velodb {

Value::Value(DataTypeId type_id, ValueData data)
    : type_id_(type_id)
    , data_(std::move(data))
    , is_null_(false)
{
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
    return std::get<std::string>(data_);
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
    default:
        return "UNKNOWN";
    }
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
    return { DataTypeId::VARCHAR, value };
}

Value Value::createNull(DataTypeId type_id)
{
    return Value { type_id };
}

} // namespace velodb
