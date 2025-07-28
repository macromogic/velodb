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

Value BinaryLogicalExpression::evaluate(const Tuple& tuple, const Schema& schema) const
{
    Value left_value = left_->evaluate(tuple, schema);
    Value right_value = right_->evaluate(tuple, schema);

    if (left_value.isNull() || right_value.isNull()) {
        return Value::createNull(DataTypeId::BOOLEAN);
    }

    bool left_bool = left_value.getBoolean();
    bool right_bool = right_value.getBoolean();

    if (connective_type_ == ConnectiveType::AND) {
        return Value::createBoolean(left_bool && right_bool);
    } else { // OR
        return Value::createBoolean(left_bool || right_bool);
    }
}

std::vector<size_t> BinaryLogicalExpression::getRequiredColumns(const Schema& schema) const
{
    std::vector<size_t> required_columns;
    auto left_columns = left_->getRequiredColumns(schema);
    required_columns.insert(required_columns.end(), left_columns.begin(), left_columns.end());
    
    auto right_columns = right_->getRequiredColumns(schema);
    required_columns.insert(required_columns.end(), right_columns.begin(), right_columns.end());
    
    // Remove duplicates
    std::sort(required_columns.begin(), required_columns.end());
    required_columns.erase(std::unique(required_columns.begin(), required_columns.end()), required_columns.end());
    
    return required_columns;
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
