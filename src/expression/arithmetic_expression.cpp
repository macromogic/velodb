#include "expression/arithmetic_expression.hpp"
#include "types/type_checker.hpp"
#include <stdexcept>
#include <algorithm>

namespace velodb {

ArithmeticExpression::ArithmeticExpression(ArithmeticType arith_type,
    std::unique_ptr<AbstractExpression> left,
    std::unique_ptr<AbstractExpression> right)
    : AbstractExpression(ExpressionType::ARITHMETIC, 
        g_type_checker.deduceArithmeticType(left->getReturnType(), right->getReturnType(), arith_type))
    , arith_type_(arith_type)
    , left_(std::move(left))
    , right_(std::move(right))
{
    // Validate the arithmetic operation at construction time
    if (!return_type_) {
        throw std::runtime_error("Invalid arithmetic operation: " + g_type_checker.getLastError());
    }
}

Value ArithmeticExpression::evaluate(const Tuple& tuple, const Schema& schema) const
{
    Value left_val = left_->evaluate(tuple, schema);
    Value right_val = right_->evaluate(tuple, schema);

    return computeArithmetic(left_val, right_val, arith_type_);
}

std::vector<size_t> ArithmeticExpression::getRequiredColumns(const Schema& schema) const
{
    std::vector<size_t> required_columns;
    
    // Get required columns from left operand
    auto left_columns = left_->getRequiredColumns(schema);
    required_columns.insert(required_columns.end(), left_columns.begin(), left_columns.end());
    
    // Get required columns from right operand
    auto right_columns = right_->getRequiredColumns(schema);
    required_columns.insert(required_columns.end(), right_columns.begin(), right_columns.end());
    
    // Remove duplicates
    std::sort(required_columns.begin(), required_columns.end());
    required_columns.erase(std::unique(required_columns.begin(), required_columns.end()), required_columns.end());
    
    return required_columns;
}

std::string ArithmeticExpression::toString() const
{
    std::string op_str;
    switch (arith_type_) {
        case ArithmeticType::PLUS:
            op_str = "+";
            break;
        case ArithmeticType::MINUS:
            op_str = "-";
            break;
        case ArithmeticType::MULTIPLY:
            op_str = "*";
            break;
        case ArithmeticType::DIVIDE:
            op_str = "/";
            break;
        case ArithmeticType::MODULO:
            op_str = "%";
            break;
    }
    
    return "(" + left_->toString() + " " + op_str + " " + right_->toString() + ")";
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
    auto result_type = g_type_checker.deduceArithmeticType(
        *DataType::createType(left_type), 
        *DataType::createType(right_type), 
        op_type
    );
    
    if (!result_type) {
        throw std::runtime_error("Invalid arithmetic operation");
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
                throw std::runtime_error("Unsupported arithmetic result type");
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("Arithmetic operation failed: " + std::string(e.what()));
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
            throw std::runtime_error("Cannot convert value to integer");
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
            throw std::runtime_error("Cannot convert value to bigint");
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
            throw std::runtime_error("Cannot convert value to double");
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
            if (right == 0) {
                throw std::runtime_error("Division by zero");
            }
            return left / right;
        case ArithmeticType::MODULO:
            if (right == 0) {
                throw std::runtime_error("Modulo by zero");
            }
            return left % right;
        default:
            throw std::runtime_error("Unknown arithmetic operation");
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
            if (right == 0) {
                throw std::runtime_error("Division by zero");
            }
            return left / right;
        case ArithmeticType::MODULO:
            if (right == 0) {
                throw std::runtime_error("Modulo by zero");
            }
            return left % right;
        default:
            throw std::runtime_error("Unknown arithmetic operation");
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
            if (right == 0.0) {
                throw std::runtime_error("Division by zero");
            }
            return left / right;
        case ArithmeticType::MODULO:
            throw std::runtime_error("Modulo operation not supported for floating-point numbers");
        default:
            throw std::runtime_error("Unknown arithmetic operation");
    }
}

} // namespace velodb
