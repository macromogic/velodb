#include "expression/constant_expression.hpp"

namespace velodb {

ConstantExpression::ConstantExpression(const Value& value)
    : LeafExpression(ExpressionType::CONSTANT, DataType::createType(value.getTypeId()))
    , value_(value)
{
}

const Value ConstantExpression::evaluate(const Tuple& /* tuple */, const Schema& /* schema */) const
{
    return value_;
}

const Value ConstantExpression::getValue() const
{
    return value_;
}

std::string ConstantExpression::toString() const
{
    return value_.toString();
}

std::unique_ptr<AbstractExpression> ConstantExpression::cloneUniqueImpl() const
{
    return std::make_unique<ConstantExpression>(value_);
}

} // namespace velodb
