#include "expression/constant_expression.hpp"

#include "catalog/column.hpp"
#include "catalog/row_batch.hpp"

namespace velodb {

ConstantExpression::ConstantExpression(const Value& value)
    : LeafExpression(ExpressionType::CONSTANT, DataType::createType(value.getTypeId()))
    , value_(value)
{
}

const Value ConstantExpression::evaluate(const Tuple& /* tuple */, const Schema& /* schema */) const
{
    return value_;
}

Column ConstantExpression::evaluateBatch(const RowBatch& batch, const Schema& /* schema */) const
{
    size_t rows = batch.getRowCount();

    // Create a vector of values and use buildFrom to ensure proper column construction
    // (especially important for dictionary-encoded types like VARCHAR)
    std::vector<Value> values;
    values.reserve(rows);
    for (size_t i = 0; i < rows; ++i) {
        values.push_back(value_);
    }
    return Column::buildFrom(getReturnType().cloneUnique(), std::move(values));
}

const Value ConstantExpression::getValue() const
{
    return value_;
}

std::string ConstantExpression::toString() const
{
    return value_.toString();
}

std::unique_ptr<AbstractExpression> ConstantExpression::cloneUniqueImpl() const
{
    return std::make_unique<ConstantExpression>(value_);
}

} // namespace velodb
