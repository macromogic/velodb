#include "expression/logical_expression.hpp"

namespace velodb {

BinaryLogicalExpression::BinaryLogicalExpression(ConnectiveType connective_type,
        std::unique_ptr<AbstractExpression> left,
        std::unique_ptr<AbstractExpression> right)
    : AbstractExpression(ExpressionType::LOGICAL, std::make_unique<BooleanType>())
    , connective_type_(connective_type)
    , left_(std::move(left))
    , right_(std::move(right))
{
}

Value BinaryLogicalExpression::evaluate([[maybe_unused]] const Tuple& tuple, [[maybe_unused]] const Schema& schema) const
{
    // TODO: Implement and/or evaluation
    return Value::createNull(DataTypeId::BOOLEAN);
}

std::vector<size_t> BinaryLogicalExpression::getRequiredColumns([[maybe_unused]] const Schema& schema) const
{
    // TODO: Implement required columns collection
    return {};
}

std::string BinaryLogicalExpression::toString() const
{
    return "(" + left_->toString() + 
           (connective_type_ == ConnectiveType::AND ? " AND " : " OR ") +
           right_->toString() + ")";
}

LogicalNotExpression::LogicalNotExpression(std::unique_ptr<AbstractExpression> operand)
    : AbstractExpression(ExpressionType::LOGICAL, std::make_unique<BooleanType>())
    , operand_(std::move(operand))
{
}

Value LogicalNotExpression::evaluate(const Tuple& tuple, const Schema& schema) const
{
    Value operand_value = operand_->evaluate(tuple, schema);
    if (operand_value.isNull()) {
        return Value::createNull(DataTypeId::BOOLEAN);
    }
    return Value::createBoolean(!operand_value.getBoolean());
}

std::vector<size_t> LogicalNotExpression::getRequiredColumns(const Schema& schema) const
{
    return operand_->getRequiredColumns(schema);
}

std::string LogicalNotExpression::toString() const
{
    return "NOT (" + operand_->toString() + ")";
}

} // namespace velodb
