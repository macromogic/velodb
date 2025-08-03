#include "expression/column_ref_expression.hpp"

#include <utility>

namespace velodb {

ColumnRefExpression::ColumnRefExpression(std::string column_name, std::unique_ptr<DataType> return_type)
    : AbstractExpression(ExpressionType::COLUMN_REF, std::move(return_type))
    , column_name_(std::move(column_name))
    , column_index_(0)
    , has_column_index_(false)
{
}

ColumnRefExpression::ColumnRefExpression(size_t column_index, std::unique_ptr<DataType> return_type)
    : AbstractExpression(ExpressionType::COLUMN_REF, std::move(return_type))
    , column_index_(column_index)
    , has_column_index_(true)
{
}

Value ColumnRefExpression::evaluate(const Tuple& tuple, const Schema& /* schema */) const
{
    if (has_column_index_) {
        return tuple.getValue(column_index_);
    }
    return tuple.getValue(column_name_);
}

std::vector<size_t> ColumnRefExpression::getRequiredColumns(const Schema& schema) const
{
    if (has_column_index_) {
        return { column_index_ };
    }
    return { schema.getColumnIndex(column_name_) };
}

std::string ColumnRefExpression::toString() const
{
    if (has_column_index_) {
        return "col_" + std::to_string(column_index_);
    }
    return column_name_;
}

} // namespace velodb
