#include "execution/expression.hpp"
#include <stdexcept>

namespace velodb {

ArithmeticExpression::ArithmeticExpression(ArithmeticType arith_type,
    std::unique_ptr<AbstractExpression> left,
    std::unique_ptr<AbstractExpression> right)
    : AbstractExpression(ExpressionType::ARITHMETIC, std::make_unique<DoubleType>())
    , arith_type_(arith_type)
    , left_(std::move(left))
    , right_(std::move(right))
{
}

Value ArithmeticExpression::evaluate([[maybe_unused]] const Tuple* tuple, [[maybe_unused]] const Schema* schema) const
{
    // TODO: Implement arithmetic evaluation
    throw std::runtime_error("ArithmeticExpression::Evaluate not implemented");
}

Value ArithmeticExpression::evaluateJoin([[maybe_unused]] const Tuple* left_tuple, [[maybe_unused]] const Schema* left_schema,
    [[maybe_unused]] const Tuple* right_tuple, [[maybe_unused]] const Schema* right_schema) const
{
    // TODO: Implement arithmetic join evaluation
    throw std::runtime_error("ArithmeticExpression::EvaluateJoin not implemented");
}

std::vector<size_t> ArithmeticExpression::getRequiredColumns([[maybe_unused]] const Schema& schema) const
{
    // TODO: Implement required columns collection
    return {};
}

std::string ArithmeticExpression::toString() const
{
    // TODO: Implement string representation
    return "ARITHMETIC_EXPR";
}

Value ArithmeticExpression::computeArithmetic([[maybe_unused]] const Value& left_val, [[maybe_unused]] const Value& right_val)
{
    // TODO: Implement arithmetic computation
    throw std::runtime_error("ArithmeticExpression::ComputeArithmetic not implemented");
}

} // namespace velodb
