#include "expression/expression.hpp"

#include <memory>

namespace velodb {

// AbstractExpression implementation
AbstractExpression::AbstractExpression(ExpressionType type, std::unique_ptr<DataType> return_type)
    : type_(type)
    , return_type_(std::move(return_type))
{
}

bool AbstractExpression::debug_flag_ = false;

LeafExpression::LeafExpression(ExpressionType type, std::unique_ptr<DataType> return_type)
    : AbstractExpression(type, std::move(return_type))
{
}

UnaryExpression::UnaryExpression(ExpressionType type,
                                 std::unique_ptr<DataType> return_type,
                                 std::unique_ptr<AbstractExpression> operand)
    : AbstractExpression(type, std::move(return_type))
    , operand_(std::move(operand))
{
}

BinaryExpression::BinaryExpression(ExpressionType type,
                                   std::unique_ptr<DataType> return_type,
                                   std::unique_ptr<AbstractExpression> left,
                                   std::unique_ptr<AbstractExpression> right)
    : AbstractExpression(type, std::move(return_type))
    , left_(std::move(left))
    , right_(std::move(right))
{
}

} // namespace velodb
