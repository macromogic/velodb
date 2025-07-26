#include "expression/conjunction_expression.hpp"

namespace velodb {

ConjunctionExpression::ConjunctionExpression(ConjunctionType conj_type,
        std::unique_ptr<AbstractExpression> left,
        std::unique_ptr<AbstractExpression> right)
    : AbstractExpression(ExpressionType::CONJUNCTION, std::make_unique<BooleanType>())
    , conj_type_(conj_type)
    , left_(std::move(left))
    , right_(std::move(right))
{
}

Value ConjunctionExpression::evaluate([[maybe_unused]] const Tuple& tuple, [[maybe_unused]] const Schema& schema) const
{
    // TODO: Implement conjunction evaluation
    return Value::createNull(DataTypeId::BOOLEAN);
}

std::vector<size_t> ConjunctionExpression::getRequiredColumns([[maybe_unused]] const Schema& schema) const
{
    // TODO: Implement required columns collection
    return {};
}

std::string ConjunctionExpression::toString() const
{
    return "(" + left_->toString() + 
           (conj_type_ == ConjunctionType::AND ? " AND " : " OR ") +
           right_->toString() + ")";
}

} // namespace velodb
