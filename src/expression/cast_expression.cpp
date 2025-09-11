#include "expression/cast_expression.hpp"

#include "common/exception.hpp"
#include "common/fmt.hpp"
#include "data/type_checker.hpp"

#include <algorithm>
#include <cctype>
#include <climits>

namespace velodb {

CastExpression::CastExpression(std::unique_ptr<AbstractExpression> operand, std::unique_ptr<DataType> target_type)
    : UnaryExpression(ExpressionType::CAST, std::move(target_type), std::move(operand))
    , target_type_(DataType::createType(return_type_->getTypeId(), return_type_->size()))
{
    // Validate the cast operation at construction time
    if (!g_type_checker.validateCast(operand_->getReturnType(), *target_type_)) {
        VELODB_THROW(TypeError, "Invalid cast operation: " + g_type_checker.getLastError());
    }
}

const Value CastExpression::evaluate(const Tuple& tuple, const Schema& schema) const
{
    Value operand_val = operand_->evaluate(tuple, schema);
    return performCast(operand_val, *target_type_);
}

std::string CastExpression::toString() const
{
    return fmt::format("CAST({} AS {})", *operand_, *target_type_);
}

Value CastExpression::performCast(const Value& value, const DataType& target_type)
{
    // Handle NULL values
    if (value.isNull()) {
        return Value::createNull(target_type.getTypeId());
    }

    DataTypeId target_type_id = target_type.getTypeId();

    try {
        switch (target_type_id) {
        case DataTypeId::BOOLEAN:
            return castToBoolean(value);
        case DataTypeId::INTEGER:
            return castToInteger(value);
        case DataTypeId::BIGINT:
            return castToBigInt(value);
        case DataTypeId::DOUBLE:
            return castToDouble(value);
        case DataTypeId::VARCHAR:
            return castToString(value);
        default:
            VELODB_THROW(TypeError, fmt::format("Unsupported cast target type: {}", target_type));
        }
    } catch (const std::exception& e) {
        VELODB_THROW(TypeError, fmt::format("Cast operation failed: {}", e.what()));
    }
}

Value CastExpression::castToBoolean(const Value& value)
{
    switch (value.getTypeId()) {
    case DataTypeId::BOOLEAN:
        return value;
    case DataTypeId::INTEGER:
        return Value::createBoolean(value.getInteger() != 0);
    case DataTypeId::BIGINT:
        return Value::createBoolean(value.getBigInt() != 0);
    case DataTypeId::DOUBLE:
        return Value::createBoolean(value.getDouble() != 0.0);
    case DataTypeId::VARCHAR: {
        std::string str = value.getString();
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        if (str == "true" || str == "t" || str == "1") {
            return Value::createBoolean(true);
        } else if (str == "false" || str == "f" || str == "0") {
            return Value::createBoolean(false);
        } else {
            VELODB_THROW(TypeError, fmt::format("Cannot convert string '{}' to boolean", value.getString()));
        }
    }
    default:
        VELODB_THROW(TypeError, fmt::format("Cannot cast {} to boolean", value));
    }
}

Value CastExpression::castToInteger(const Value& value)
{
    switch (value.getTypeId()) {
    case DataTypeId::BOOLEAN:
        return Value::createInteger(value.getBoolean() ? 1 : 0);
    case DataTypeId::INTEGER:
        return value;
    case DataTypeId::BIGINT: {
        int64_t bigint_val = value.getBigInt();
        if (bigint_val > INT32_MAX || bigint_val < INT32_MIN) {
            VELODB_THROW(TypeError, "BigInt value out of range for Integer");
        }
        return Value::createInteger(static_cast<int32_t>(bigint_val));
    }
    case DataTypeId::DOUBLE: {
        double double_val = value.getDouble();
        if (double_val > INT32_MAX || double_val < INT32_MIN) {
            VELODB_THROW(TypeError, "Double value out of range for Integer");
        }
        return Value::createInteger(static_cast<int32_t>(double_val));
    }
    case DataTypeId::VARCHAR: {
        try {
            int32_t int_val = std::stoi(value.getString());
            return Value::createInteger(int_val);
        } catch (const std::exception&) {
            VELODB_THROW(TypeError, fmt::format("Cannot convert string '{}' to integer", value.getString()));
        }
    }
    default:
        VELODB_THROW(TypeError, fmt::format("Cannot cast {} to integer", value));
    }
}

Value CastExpression::castToBigInt(const Value& value)
{
    switch (value.getTypeId()) {
    case DataTypeId::BOOLEAN:
        return Value::createBigInt(value.getBoolean() ? 1L : 0L);
    case DataTypeId::INTEGER:
        return Value::createBigInt(static_cast<int64_t>(value.getInteger()));
    case DataTypeId::BIGINT:
        return value;
    case DataTypeId::DOUBLE: {
        double double_val = value.getDouble();
        if (double_val > INT64_MAX || double_val < INT64_MIN) {
            VELODB_THROW(TypeError, "Double value out of range for BigInt");
        }
        return Value::createBigInt(static_cast<int64_t>(double_val));
    }
    case DataTypeId::VARCHAR: {
        try {
            int64_t bigint_val = std::stoll(value.getString());
            return Value::createBigInt(bigint_val);
        } catch (const std::exception&) {
            VELODB_THROW(TypeError, fmt::format("Cannot convert string '{}' to bigint", value.getString()));
        }
    }
    default:
        VELODB_THROW(TypeError, fmt::format("Cannot cast {} to bigint", value));
    }
}

Value CastExpression::castToDouble(const Value& value)
{
    switch (value.getTypeId()) {
    case DataTypeId::BOOLEAN:
        return Value::createDouble(value.getBoolean() ? 1.0 : 0.0);
    case DataTypeId::INTEGER:
        return Value::createDouble(static_cast<double>(value.getInteger()));
    case DataTypeId::BIGINT:
        return Value::createDouble(static_cast<double>(value.getBigInt()));
    case DataTypeId::DOUBLE:
        return value;
    case DataTypeId::VARCHAR: {
        try {
            double double_val = std::stod(value.getString());
            return Value::createDouble(double_val);
        } catch (const std::exception&) {
            VELODB_THROW(TypeError, fmt::format("Cannot convert string '{}' to double", value.getString()));
        }
    }
    default:
        VELODB_THROW(TypeError, fmt::format("Cannot cast {} to double", value));
    }
}

Value CastExpression::castToString(const Value& value)
{
    switch (value.getTypeId()) {
    case DataTypeId::BOOLEAN:
        return Value::createString(value.getBoolean() ? "true" : "false");
    case DataTypeId::INTEGER:
        return Value::createString(std::to_string(value.getInteger()));
    case DataTypeId::BIGINT:
        return Value::createString(std::to_string(value.getBigInt()));
    case DataTypeId::DOUBLE:
        return Value::createString(std::to_string(value.getDouble()));
    case DataTypeId::VARCHAR:
        return value;
    default:
        return Value::createString(value.toString());
    }
}

std::unique_ptr<AbstractExpression> CastExpression::cloneUniqueImpl() const
{
    return std::make_unique<CastExpression>(operand_->cloneUnique(), target_type_->cloneUnique());
}

} // namespace velodb
