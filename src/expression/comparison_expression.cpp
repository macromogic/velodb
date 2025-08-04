#include "expression/comparison_expression.hpp"

#include "common/exception.hpp"
#include "common/fmt.hpp"

#include <fmt/format.h>

#include <stdexcept>

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
    : AbstractExpression(ExpressionType::COMPARISON, std::make_unique<BooleanType>())
    , comp_type_(comp_type)
    , left_(std::move(left))
    , right_(std::move(right))
{
}

Value ComparisonExpression::evaluate(const Tuple& tuple, const Schema& schema) const
{
    Value const left_val = left_->evaluate(tuple, schema);
    Value const right_val = right_->evaluate(tuple, schema);
    return compareValues(left_val, right_val);
}

std::vector<size_t> ComparisonExpression::getRequiredColumns(const Schema& schema) const
{
    auto left_cols = left_->getRequiredColumns(schema);
    auto right_cols = right_->getRequiredColumns(schema);
    left_cols.insert(left_cols.end(), right_cols.begin(), right_cols.end());
    return left_cols;
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

    return Value::createBoolean(result);
}

} // namespace velodb
