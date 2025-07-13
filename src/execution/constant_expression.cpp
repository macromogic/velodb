#include "execution/expression.hpp"

namespace velodb {

ConstantExpression::ConstantExpression(const Value& value)
    : AbstractExpression(ExpressionType::CONSTANT,
        DataType::createType(value.getTypeId()))
    , value_(value)
{
}

Value ConstantExpression::evaluate([[maybe_unused]] const Tuple* tuple, [[maybe_unused]] const Schema* schema) const
{
    return value_;
}

std::vector<size_t> ConstantExpression::getRequiredColumns([[maybe_unused]] const Schema& schema) const
{
    return {}; // Constants don't require any columns
}

std::string ConstantExpression::toString() const
{
    return value_.toString();
}

} // namespace velodb
