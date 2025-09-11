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

const Value FunctionCallExpression::evaluate([[maybe_unused]] const Tuple& tuple,
                                             [[maybe_unused]] const Schema& schema) const
{
    // TODO: Implement function call evaluation logic
    VELODB_THROW(ExecutionError, "FunctionCallExpression::evaluate not implemented yet");
}

std::string FunctionCallExpression::toString() const
{
    return function_name_ + "(...)";
}

std::unique_ptr<AbstractExpression> FunctionCallExpression::cloneUniqueImpl() const
{
    std::vector<std::unique_ptr<AbstractExpression>> cloned_args;
    cloned_args.reserve(arguments_.size());
    for (const auto& arg : arguments_) {
        cloned_args.push_back(arg->cloneUnique());
    }
    return std::make_unique<FunctionCallExpression>(function_name_,
                                                    std::move(cloned_args),
                                                    return_type_->cloneUnique());
}

} // namespace velodb
