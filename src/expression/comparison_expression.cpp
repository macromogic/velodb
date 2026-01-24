#include "expression/comparison_expression.hpp"

#include "common/exception.hpp"
#include "common/fmt.hpp"

namespace velodb {

static auto format_as(ComparisonType comp_type)
{
    switch (comp_type) {
    case ComparisonType::EQUAL:
        return "=";
    case ComparisonType::NOT_EQUAL:
        return "!=";
    case ComparisonType::LESS_THAN:
        return "<";
    case ComparisonType::LESS_THAN_OR_EQUAL:
        return "<=";
    case ComparisonType::GREATER_THAN:
        return ">";
    case ComparisonType::GREATER_THAN_OR_EQUAL:
        return ">=";
    default:
        return "(unknown)";
    }
}

ComparisonExpression::ComparisonExpression(ComparisonType comp_type,
                                           std::unique_ptr<AbstractExpression> left,
                                           std::unique_ptr<AbstractExpression> right)
    : BinaryExpression(ExpressionType::COMPARISON, std::make_unique<BooleanType>(), std::move(left), std::move(right))
    , comp_type_(comp_type)
{
}

const Value ComparisonExpression::evaluate(const Tuple& tuple, const Schema& schema) const
{
    Value const left_val = left_->evaluate(tuple, schema);
    Value const right_val = right_->evaluate(tuple, schema);
    return compareValues(left_val, right_val);
}

std::string ComparisonExpression::toString() const
{
    return fmt::format("({} {} {})", *left_, comp_type_, *right_);
}

Value ComparisonExpression::compareValues(const Value& left_val, const Value& right_val) const
{
    if (left_val.isNull() || right_val.isNull()) {
        return Value::createNull(DataTypeId::BOOLEAN);
    }

    bool result = false;
    switch (comp_type_) {
    case ComparisonType::EQUAL:
        result = (left_val == right_val);
        break;
    case ComparisonType::NOT_EQUAL:
        result = (left_val != right_val);
        break;
    case ComparisonType::LESS_THAN:
        result = (left_val < right_val);
        break;
    case ComparisonType::LESS_THAN_OR_EQUAL:
        result = (left_val <= right_val);
        break;
    case ComparisonType::GREATER_THAN:
        result = (left_val > right_val);
        break;
    case ComparisonType::GREATER_THAN_OR_EQUAL:
        result = (left_val >= right_val);
        break;
    default:
        VELODB_THROW(ExecutionError, fmt::format("Comparison operator {} not implemented", comp_type_));
    }

    if (debug_flag_) {
        fmt::println("Comparing values: {}({}) {} {}({}) => {}",
                     left_val,
                     DataType::createType(left_val.getTypeId()),
                     comp_type_,
                     right_val,
                     DataType::createType(right_val.getTypeId()),
                     result);
    }
    return Value::createBoolean(result);
}

std::unique_ptr<AbstractExpression> ComparisonExpression::cloneUniqueImpl() const
{
    return std::make_unique<ComparisonExpression>(comp_type_, left_->cloneUnique(), right_->cloneUnique());
}

} // namespace velodb
