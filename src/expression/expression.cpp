#include "expression/expression.hpp"
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

// Concrete expression implementations are now in separate files

} // namespace velodb
