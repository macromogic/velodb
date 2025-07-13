#include "execution/expression.hpp"
#include <stdexcept>
#include <utility>

namespace velodb {

FunctionCallExpression::FunctionCallExpression(std::string function_name,
    std::vector<std::unique_ptr<AbstractExpression>> arguments,
    std::unique_ptr<DataType> return_type)
    : AbstractExpression(ExpressionType::FUNCTION_CALL, std::move(return_type))
    , function_name_(std::move(function_name))
    , arguments_(std::move(arguments))
{
}

Value FunctionCallExpression::evaluate([[maybe_unused]] const Tuple* tuple, [[maybe_unused]] const Schema* schema) const
{
    // TODO: Implement function call evaluation
    throw std::runtime_error("FunctionCallExpression::evaluate not implemented");
}

std::vector<size_t> FunctionCallExpression::getRequiredColumns([[maybe_unused]] const Schema& schema) const
{
    // TODO: Implement required columns collection
    return {};
}

std::string FunctionCallExpression::toString() const
{
    return function_name_ + "(...)";
}

} // namespace velodb
