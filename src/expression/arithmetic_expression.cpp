#include "expression/arithmetic_expression.hpp"

#include "common/exception.hpp"
#include "common/fmt.hpp"
#include "data/type_checker.hpp"

#include <algorithm>

namespace velodb {

static auto format_as(ArithmeticType arith_type)
{
    switch (arith_type) {
    case ArithmeticType::PLUS:
        return "+";
    case ArithmeticType::MINUS:
        return "-";
    case ArithmeticType::MULTIPLY:
        return "*";
    case ArithmeticType::DIVIDE:
        return "/";
    case ArithmeticType::MODULO:
        return "%";
    default:
        return "(unknown)";
    }
}

ArithmeticExpression::ArithmeticExpression(ArithmeticType arith_type,
                                           std::unique_ptr<DataType> return_type,
                                           std::unique_ptr<AbstractExpression> left,
                                           std::unique_ptr<AbstractExpression> right)
    : BinaryExpression(ExpressionType::ARITHMETIC, std::move(return_type), std::move(left), std::move(right))
    , arith_type_(arith_type)
{
    // Validate the arithmetic operation at construction time
    if (!return_type_) {
        VELODB_THROW(TypeError, "Invalid arithmetic operation: " + g_type_checker.getLastError());
    }
}

const Value ArithmeticExpression::evaluate(const Tuple& tuple, const Schema& schema) const
{
    Value left_val = left_->evaluate(tuple, schema);
    Value right_val = right_->evaluate(tuple, schema);

    return computeArithmetic(left_val, right_val, arith_type_);
}

std::string ArithmeticExpression::toString() const
{
    return fmt::format("({} {} {})", *left_, arith_type_, *right_);
}

Value ArithmeticExpression::computeArithmetic(const Value& left_val, const Value& right_val, ArithmeticType op_type)
{
    // Handle NULL values
    if (left_val.isNull() || right_val.isNull()) {
        return Value::createNull(DataTypeId::INVALID);
    }

    // Get the operand types
    DataTypeId left_type = left_val.getTypeId();
    DataTypeId right_type = right_val.getTypeId();

    // Determine result type using type checker
    auto result_type = g_type_checker.deduceArithmeticType(*DataType::createType(left_type),
                                                           *DataType::createType(right_type),
                                                           op_type);

    if (!result_type) {
        VELODB_THROW(TypeError, "Invalid arithmetic operation");
    }

    // Perform the computation based on result type
    DataTypeId result_type_id = result_type->getTypeId();

    try {
        switch (result_type_id) {
        case DataTypeId::INTEGER: {
            int32_t left_int = convertToInteger(left_val);
            int32_t right_int = convertToInteger(right_val);
            int32_t result = performIntegerArithmetic(left_int, right_int, op_type);
            return Value::createInteger(result);
        }
        case DataTypeId::BIGINT: {
            int64_t left_bigint = convertToBigInt(left_val);
            int64_t right_bigint = convertToBigInt(right_val);
            int64_t result = performBigIntArithmetic(left_bigint, right_bigint, op_type);
            return Value::createBigInt(result);
        }
        case DataTypeId::DOUBLE: {
            double left_double = convertToDouble(left_val);
            double right_double = convertToDouble(right_val);
            double result = performDoubleArithmetic(left_double, right_double, op_type);
            return Value::createDouble(result);
        }
        default:
            VELODB_THROW(TypeError, "Unsupported arithmetic result type");
        }
    } catch (const std::exception& e) {
        VELODB_THROW(ExecutionError, "Arithmetic operation failed: " + std::string(e.what()));
    }
}

int32_t ArithmeticExpression::convertToInteger(const Value& val)
{
    switch (val.getTypeId()) {
    case DataTypeId::INTEGER:
        return val.getInteger();
    case DataTypeId::BIGINT:
        return static_cast<int32_t>(val.getBigInt());
    case DataTypeId::DOUBLE:
        return static_cast<int32_t>(val.getDouble());
    default:
        VELODB_THROW(TypeError, "Cannot convert value to integer");
    }
}

int64_t ArithmeticExpression::convertToBigInt(const Value& val)
{
    switch (val.getTypeId()) {
    case DataTypeId::INTEGER:
        return static_cast<int64_t>(val.getInteger());
    case DataTypeId::BIGINT:
        return val.getBigInt();
    case DataTypeId::DOUBLE:
        return static_cast<int64_t>(val.getDouble());
    default:
        VELODB_THROW(TypeError, "Cannot convert value to bigint");
    }
}

double ArithmeticExpression::convertToDouble(const Value& val)
{
    switch (val.getTypeId()) {
    case DataTypeId::INTEGER:
        return static_cast<double>(val.getInteger());
    case DataTypeId::BIGINT:
        return static_cast<double>(val.getBigInt());
    case DataTypeId::DOUBLE:
        return val.getDouble();
    default:
        VELODB_THROW(TypeError, "Cannot convert value to double");
    }
}

int32_t ArithmeticExpression::performIntegerArithmetic(int32_t left, int32_t right, ArithmeticType op_type)
{
    switch (op_type) {
    case ArithmeticType::PLUS:
        return left + right;
    case ArithmeticType::MINUS:
        return left - right;
    case ArithmeticType::MULTIPLY:
        return left * right;
    case ArithmeticType::DIVIDE:
        VELODB_ASSERT_MSG(right != 0, "Division by zero");
        return left / right;
    case ArithmeticType::MODULO:
        VELODB_ASSERT_MSG(right != 0, "Modulo by zero");
        return left % right;
    default:
        VELODB_THROW(ExecutionError, "Unknown arithmetic operation");
    }
}

int64_t ArithmeticExpression::performBigIntArithmetic(int64_t left, int64_t right, ArithmeticType op_type)
{
    switch (op_type) {
    case ArithmeticType::PLUS:
        return left + right;
    case ArithmeticType::MINUS:
        return left - right;
    case ArithmeticType::MULTIPLY:
        return left * right;
    case ArithmeticType::DIVIDE:
        VELODB_ASSERT_MSG(right != 0, "Division by zero");
        return left / right;
    case ArithmeticType::MODULO:
        VELODB_ASSERT_MSG(right != 0, "Modulo by zero");
        return left % right;
    default:
        VELODB_THROW(ExecutionError, "Unknown arithmetic operation");
    }
}

double ArithmeticExpression::performDoubleArithmetic(double left, double right, ArithmeticType op_type)
{
    switch (op_type) {
    case ArithmeticType::PLUS:
        return left + right;
    case ArithmeticType::MINUS:
        return left - right;
    case ArithmeticType::MULTIPLY:
        return left * right;
    case ArithmeticType::DIVIDE:
        VELODB_ASSERT_MSG(right != 0.0, "Division by zero");
        return left / right;
    case ArithmeticType::MODULO:
        VELODB_THROW(ExecutionError, "Modulo operation not supported for floating-point numbers");
    default:
        VELODB_THROW(ExecutionError, "Unknown arithmetic operation");
    }
}

std::unique_ptr<AbstractExpression> ArithmeticExpression::cloneUniqueImpl() const
{
    return std::make_unique<ArithmeticExpression>(arith_type_,
                                                  return_type_->cloneUnique(),
                                                  left_->cloneUnique(),
                                                  right_->cloneUnique());
}

} // namespace velodb
