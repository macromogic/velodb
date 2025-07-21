#include "execution/expression.hpp"
#include "execution/type_checker.hpp"
#include <stdexcept>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cctype>
#include <climits>

namespace velodb {

CastExpression::CastExpression(std::unique_ptr<AbstractExpression> operand, std::unique_ptr<DataType> target_type)
    : AbstractExpression(ExpressionType::CAST, std::move(target_type))
    , operand_(std::move(operand))
    , target_type_(DataType::createType(return_type_->getTypeId(), return_type_->getSize()))
{
    // Validate the cast operation at construction time
    if (!g_type_checker.validateCast(operand_->getReturnType(), *target_type_)) {
        throw std::runtime_error("Invalid cast operation: " + g_type_checker.getLastError());
    }
}

Value CastExpression::evaluate(const Tuple* tuple, const Schema* schema) const
{
    Value operand_val = operand_->evaluate(tuple, schema);
    return performCast(operand_val, *target_type_);
}

Value CastExpression::evaluateJoin(const Tuple* left_tuple, const Schema* left_schema,
    const Tuple* right_tuple, const Schema* right_schema) const
{
    Value operand_val = operand_->evaluateJoin(left_tuple, left_schema, right_tuple, right_schema);
    return performCast(operand_val, *target_type_);
}

std::vector<size_t> CastExpression::getRequiredColumns(const Schema& schema) const
{
    return operand_->getRequiredColumns(schema);
}

std::string CastExpression::toString() const
{
    return "CAST(" + operand_->toString() + " AS " + target_type_->toString() + ")";
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
            case DataTypeId::DATE:
                return castToDate(value);
            case DataTypeId::TIMESTAMP:
                return castToTimestamp(value);
            default:
                throw std::runtime_error("Unsupported cast target type: " + target_type.toString());
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("Cast operation failed: " + std::string(e.what()));
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
                throw std::runtime_error("Cannot convert string '" + value.getString() + "' to boolean");
            }
        }
        default:
            throw std::runtime_error("Cannot cast " + value.toString() + " to boolean");
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
                throw std::runtime_error("BigInt value out of range for Integer");
            }
            return Value::createInteger(static_cast<int32_t>(bigint_val));
        }
        case DataTypeId::DOUBLE: {
            double double_val = value.getDouble();
            if (double_val > INT32_MAX || double_val < INT32_MIN) {
                throw std::runtime_error("Double value out of range for Integer");
            }
            return Value::createInteger(static_cast<int32_t>(double_val));
        }
        case DataTypeId::VARCHAR: {
            try {
                int32_t int_val = std::stoi(value.getString());
                return Value::createInteger(int_val);
            } catch (const std::exception&) {
                throw std::runtime_error("Cannot convert string '" + value.getString() + "' to integer");
            }
        }
        default:
            throw std::runtime_error("Cannot cast " + value.toString() + " to integer");
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
                throw std::runtime_error("Double value out of range for BigInt");
            }
            return Value::createBigInt(static_cast<int64_t>(double_val));
        }
        case DataTypeId::VARCHAR: {
            try {
                int64_t bigint_val = std::stoll(value.getString());
                return Value::createBigInt(bigint_val);
            } catch (const std::exception&) {
                throw std::runtime_error("Cannot convert string '" + value.getString() + "' to bigint");
            }
        }
        default:
            throw std::runtime_error("Cannot cast " + value.toString() + " to bigint");
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
                throw std::runtime_error("Cannot convert string '" + value.getString() + "' to double");
            }
        }
        default:
            throw std::runtime_error("Cannot cast " + value.toString() + " to double");
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

Value CastExpression::castToDate(const Value& value)
{
    switch (value.getTypeId()) {
        case DataTypeId::VARCHAR: {
            // Basic date parsing - in a real implementation, you'd use a proper date library
            std::string date_str = value.getString();
            std::regex date_pattern(R"(\d{4}-\d{2}-\d{2})");
            if (std::regex_match(date_str, date_pattern)) {
                return Value::createString(date_str); // Simplified - store as string for now
            } else {
                throw std::runtime_error("Invalid date format: " + date_str);
            }
        }
        case DataTypeId::TIMESTAMP:
            // Extract date part from timestamp
            return Value::createString(value.getString().substr(0, 10)); // Simplified
        default:
            throw std::runtime_error("Cannot cast " + value.toString() + " to date");
    }
}

Value CastExpression::castToTimestamp(const Value& value)
{
    switch (value.getTypeId()) {
        case DataTypeId::VARCHAR: {
            // Basic timestamp parsing
            std::string timestamp_str = value.getString();
            std::regex timestamp_pattern(R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})");
            if (std::regex_match(timestamp_str, timestamp_pattern)) {
                return Value::createString(timestamp_str); // Simplified - store as string for now
            } else {
                throw std::runtime_error("Invalid timestamp format: " + timestamp_str);
            }
        }
        case DataTypeId::DATE:
            // Add default time to date
            return Value::createString(value.getString() + " 00:00:00"); // Simplified
        default:
            throw std::runtime_error("Cannot cast " + value.toString() + " to timestamp");
    }
}

} // namespace velodb
