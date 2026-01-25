#include "expression/column_ref_expression.hpp"

#include "catalog/column.hpp"
#include "catalog/row_batch.hpp"
#include "catalog/tuple.hpp"

#include <fmt/format.h>

#include <utility>

namespace velodb {

ColumnRefExpression::ColumnRefExpression(std::string table_name,
                                         std::string column_name,
                                         std::unique_ptr<DataType> return_type)
    : LeafExpression(ExpressionType::COLUMN_REF, std::move(return_type))
    , table_name_(std::move(table_name))
    , column_name_(std::move(column_name))
{
}

const Value ColumnRefExpression::evaluate(const Tuple& tuple, const Schema& schema) const
{
    auto column_index = schema.getColumnIndex(column_name_);
    return tuple.getValue(column_index);
}

Column ColumnRefExpression::evaluateBatch(const RowBatch& batch, const Schema& schema) const
{
    auto column_index = schema.getColumnIndex(column_name_);
    return batch.getColumn(column_index).slice(0, batch.getRowCount());
}

std::string ColumnRefExpression::toString() const
{
    return column_name_;
}

std::unique_ptr<AbstractExpression> ColumnRefExpression::cloneUniqueImpl() const
{
    return std::make_unique<ColumnRefExpression>(table_name_, column_name_, return_type_->cloneUnique());
}

} // namespace velodb
