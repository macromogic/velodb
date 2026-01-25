#include "expression/comparison_expression.hpp"

#include "catalog/column.hpp"
#include "catalog/row_batch.hpp"
#include "common/exception.hpp"
#include "common/fmt.hpp"

namespace velodb {

template <typename T>
void computeComparison(const Column& left, const Column& right, Column& result, ComparisonType op, size_t count)
{
    const T* left_data = static_cast<const T*>(left.rawData());
    const T* right_data = static_cast<const T*>(right.rawData());
    const auto* left_nulls = left.rawBitmapData();
    const auto* right_nulls = right.rawBitmapData();

    for (size_t i = 0; i < count; ++i) {
        if ((left_nulls && left_nulls[i]) || (right_nulls && right_nulls[i])) {
            result.append(Value::createNull(DataTypeId::BOOLEAN));
            continue;
        }

        bool res = false;
        T l = left_data[i];
        T r = right_data[i];
        switch (op) {
        case ComparisonType::EQUAL:
            res = (l == r);
            break;
        case ComparisonType::NOT_EQUAL:
            res = (l != r);
            break;
        case ComparisonType::LESS_THAN:
            res = (l < r);
            break;
        case ComparisonType::LESS_THAN_OR_EQUAL:
            res = (l <= r);
            break;
        case ComparisonType::GREATER_THAN:
            res = (l > r);
            break;
        case ComparisonType::GREATER_THAN_OR_EQUAL:
            res = (l >= r);
            break;
        default:
            break;
        }
        result.append(Value::createBoolean(res));
    }
}

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

Column ComparisonExpression::evaluateBatch(const RowBatch& batch, const Schema& schema) const
{
    Column left_col = left_->evaluateBatch(batch, schema);
    Column right_col = right_->evaluateBatch(batch, schema);

    Column result(DataType::createType(DataTypeId::BOOLEAN), batch.getRowCount());
    size_t count = batch.getRowCount();

    if (left_col.getType().getTypeId() == right_col.getType().getTypeId()) {
        switch (left_col.getType().getTypeId()) {
        case DataTypeId::INTEGER:
            computeComparison<int32_t>(left_col, right_col, result, comp_type_, count);
            return result;
        case DataTypeId::BIGINT:
            computeComparison<int64_t>(left_col, right_col, result, comp_type_, count);
            return result;
        case DataTypeId::DOUBLE:
            computeComparison<double>(left_col, right_col, result, comp_type_, count);
            return result;
        default:
            break;
        }
    }

    // Fallback using Value API
    for (size_t i = 0; i < count; ++i) {
        result.append(compareValues(left_col.get(i), right_col.get(i)));
    }
    return result;
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
