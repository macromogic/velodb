#include "types/value.hpp"
#include <sstream>
#include <stdexcept>
#include <utility>

namespace velodb {

Value::Value(DataTypeId type_id, ValueData data)
    : type_id_(type_id)
    , data_(std::move(std::move(data)))
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
        throw std::runtime_error("Cannot get value from NULL");
    if (type_id_ != DataTypeId::BOOLEAN)
        throw std::runtime_error("Type mismatch");
    return std::get<bool>(data_);
}

int32_t Value::getInteger() const
{
    if (is_null_)
        throw std::runtime_error("Cannot get value from NULL");
    if (type_id_ != DataTypeId::INTEGER)
        throw std::runtime_error("Type mismatch");
    return std::get<int32_t>(data_);
}

int64_t Value::getBigInt() const
{
    if (is_null_)
        throw std::runtime_error("Cannot get value from NULL");
    if (type_id_ != DataTypeId::BIGINT)
        throw std::runtime_error("Type mismatch");
    return std::get<int64_t>(data_);
}

double Value::getDouble() const
{
    if (is_null_)
        throw std::runtime_error("Cannot get value from NULL");
    if (type_id_ != DataTypeId::DOUBLE)
        throw std::runtime_error("Type mismatch");
    return std::get<double>(data_);
}

std::string Value::getString() const
{
    if (is_null_)
        throw std::runtime_error("Cannot get value from NULL");
    if (type_id_ != DataTypeId::VARCHAR)
        throw std::runtime_error("Type mismatch");
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
    if (is_null_ || other.is_null_)
        return false;
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

    std::stringstream ss;
    switch (type_id_) {
    case DataTypeId::BOOLEAN:
        ss << (getBoolean() ? "true" : "false");
        break;
    case DataTypeId::INTEGER:
        ss << getInteger();
        break;
    case DataTypeId::BIGINT:
        ss << getBigInt();
        break;
    case DataTypeId::DOUBLE:
        ss << getDouble();
        break;
    case DataTypeId::VARCHAR:
        ss << "'" << getString() << "'";
        break;
    default:
        ss << "UNKNOWN";
    }
    return ss.str();
}

Value Value::createBoolean(bool value)
{
    return { DataTypeId::BOOLEAN, value };
}

Value Value::createInteger(int32_t value)
{
    return { DataTypeId::INTEGER, value };
}

Value Value::createBigInt(int64_t value)
{
    return { DataTypeId::BIGINT, value };
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
