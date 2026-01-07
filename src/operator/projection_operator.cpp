#include "operator/projection_operator.hpp"

#include "catalog/table.hpp"
#include "common/result.hpp"
#include "expression/column_ref_expression.hpp"

#include <fmt/format.h>

namespace velodb {

// ProjectionOperator implementation
ProjectionOperator::ProjectionOperator(ExecutionContext& context,
                                       Schema input_schema,
                                       Schema output_schema,
                                       std::unique_ptr<AbstractOperator> child,
                                       std::vector<std::unique_ptr<AbstractExpression>> expressions)
    : UnaryOperator(context, std::move(output_schema), std::move(child))
    , input_schema_(std::move(input_schema))
    , expressions_(std::move(expressions))
{
}

Result<RowBatch> ProjectionOperator::next()
{
    PROFILE_SCOPE("ProjectionOperator::next");
    auto* child = getChild();
    if (!child) {
        return Result<RowBatch>::failure("ProjectionOperator requires a child operator");
    }
    auto child_result = child->next();
    if (!child_result) {
        return child_result; // Propagate error from child
    }
    auto& child_batch = child_result.value();
    if (child_batch.getRowCount() == 0) {
        return child_result; // No rows to process
    }

    if (expressions_.empty()) {
        return child_result;
    } else {
        RowBatch batch;
        ViewTuple dummy_tuple(child_batch, 0);
        size_t batch_size = child_batch.getRowCount();
        for (const auto& expr : expressions_) {
            switch (expr->getExpressionType()) {
            case ExpressionType::COLUMN_REF: {
                const auto* column_ref = static_cast<ColumnRefExpression*>(expr.get());
                auto column_index = input_schema_.getColumnIndex(column_ref->getColumnName());
                batch.addColumn(child_batch.getColumn(column_index).tryOwn());
                break;
            }
            case ExpressionType::CONSTANT: {
                // TODO: faster way to create constant column
                auto values = std::vector<Value>(batch_size, expr->evaluate(dummy_tuple, getOutputSchema()));
                batch.addColumn(Column::buildFrom(expr->getReturnType().cloneUnique(), std::move(values)));
                break;
            }
            default:
                VELODB_THROW(ExecutionError, fmt::format("Unsupported expression type in projection: {}", *expr));
                break;
            }
        }
        return Result<RowBatch>::success(std::move(batch));
    }
}

} // namespace velodb
