#include "expression/constant_expression.hpp"

#include "catalog/column.hpp"
#include "catalog/row_batch.hpp"
#include "data/type_traits.hpp"

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

    std::vector<Value> value_v { value_ };
    auto col = Column::buildFrom(getReturnType().cloneUnique(), std::move(value_v));
    col.reserve(nextPow2(rows));
    void* data_ptr = col.rawData();
    switch (value_.getTypeId()) {
#define X(name, DT, VT)                                                                                                \
    case DataTypeId::name: {                                                                                           \
        DT* d = static_cast<DT*>(data_ptr);                                                                            \
        std::fill_n(d, rows, d[0]);                                                                                    \
        break;                                                                                                         \
    }
        LIST_TYPES(X)
#undef X
    default:
        VELODB_THROW(ExecutionError, "Unsupported data type in ConstantExpression::evaluateBatch");
    }
    setSizeForColumn(col, rows);
    return col;
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
