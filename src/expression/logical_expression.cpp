#include "expression/logical_expression.hpp"

#include "common/fmt.hpp"

#include <fmt/format.h>

namespace velodb {

BinaryLogicalExpression::BinaryLogicalExpression(ConnectiveType connective_type,
                                                 std::unique_ptr<AbstractExpression> left,
                                                 std::unique_ptr<AbstractExpression> right)
    : BinaryExpression(ExpressionType::LOGICAL, std::make_unique<BooleanType>(), std::move(left), std::move(right))
    , connective_type_(connective_type)
{
}

const Value BinaryLogicalExpression::evaluate(const Tuple& tuple, const Schema& schema) const
{
    Value left_value = left_->evaluate(tuple, schema);
    Value right_value = right_->evaluate(tuple, schema);

    if (left_value.isNull() || right_value.isNull()) {
        return Value::createNull(DataTypeId::BOOLEAN);
    }

    bool left_bool = left_value.getBoolean();
    bool right_bool = right_value.getBoolean();

    if (connective_type_ == ConnectiveType::AND) {
        if (debug_flag_) {
            fmt::println("Evaluating AND: {} AND {} => {}", left_bool, right_bool, left_bool && right_bool);
        }
        return Value::createBoolean(left_bool && right_bool);
    } else { // OR
        if (debug_flag_) {
            fmt::println("Evaluating OR: {} OR {} => {}", left_bool, right_bool, left_bool || right_bool);
        }
        return Value::createBoolean(left_bool || right_bool);
    }
}

std::string BinaryLogicalExpression::toString() const
{
    std::string op_str = (connective_type_ == ConnectiveType::AND) ? " AND " : " OR ";
    return fmt::format("({} {} {})", left_->toString(), op_str, right_->toString());
}

std::unique_ptr<AbstractExpression> BinaryLogicalExpression::cloneUniqueImpl() const
{
    return std::make_unique<BinaryLogicalExpression>(connective_type_, left_->cloneUnique(), right_->cloneUnique());
}

// LogicalNotExpression implementation

LogicalNotExpression::LogicalNotExpression(std::unique_ptr<AbstractExpression> operand)
    : UnaryExpression(ExpressionType::LOGICAL, std::make_unique<BooleanType>(), std::move(operand))
{
}

const Value LogicalNotExpression::evaluate(const Tuple& tuple, const Schema& schema) const
{
    Value operand_value = operand_->evaluate(tuple, schema);
    if (operand_value.isNull()) {
        return Value::createNull(DataTypeId::BOOLEAN);
    }
    return Value::createBoolean(!operand_value.getBoolean());
}

std::string LogicalNotExpression::toString() const
{
    return fmt::format("NOT ({})", *operand_);
}

std::unique_ptr<AbstractExpression> LogicalNotExpression::cloneUniqueImpl() const
{
    return std::make_unique<LogicalNotExpression>(operand_->cloneUnique());
}

} // namespace velodb
