#include "expression/function_call_expression.hpp"

#include "common/exception.hpp"

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

Value FunctionCallExpression::evaluate([[maybe_unused]] const Tuple& tuple, [[maybe_unused]] const Schema& schema) const
{
    // TODO: Implement function call evaluation logic
    VELODB_THROW(ExecutionError, "FunctionCallExpression::evaluate not implemented yet");
}

std::vector<size_t> FunctionCallExpression::getRequiredColumns(const Schema& schema) const
{
    std::vector<size_t> required_columns;
    for (const auto& arg : arguments_) {
        auto arg_columns = arg->getRequiredColumns(schema);
        required_columns.insert(required_columns.end(), arg_columns.begin(), arg_columns.end());
    }
    // Remove duplicates
    std::sort(required_columns.begin(), required_columns.end());
    required_columns.erase(std::unique(required_columns.begin(), required_columns.end()), required_columns.end());
    return required_columns;
}

std::string FunctionCallExpression::toString() const
{
    return function_name_ + "(...)";
}

} // namespace velodb
