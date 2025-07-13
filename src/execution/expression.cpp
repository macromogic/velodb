#include "execution/expression.hpp"
#include <sstream>
#include <stdexcept>

namespace velodb {

// TODO: Implement full expression evaluation system

// AbstractExpression implementation
AbstractExpression::AbstractExpression(ExpressionType type, std::unique_ptr<DataType> return_type)
    : type_(type)
    , return_type_(std::move(return_type))
{
}

Value AbstractExpression::evaluateJoin(const Tuple* left_tuple, const Schema* left_schema,
    [[maybe_unused]] const Tuple* right_tuple,
    [[maybe_unused]] const Schema* right_schema) const
{
    // TODO: Implement join evaluation - for now, just use left tuple
    return evaluate(left_tuple, left_schema);
}

// Concrete expression implementations are now in separate files

} // namespace velodb
