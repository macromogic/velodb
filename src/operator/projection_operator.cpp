#include "operator/projection_operator.hpp"

#include "catalog/table.hpp"
#include "common/result.hpp"
#include "execution/execution_engine.hpp"
#include "expression/expression.hpp"
#include "operator/scan_filter_operator.hpp"

namespace velodb {

// ProjectionOperator implementation
ProjectionOperator::ProjectionOperator(Catalog& catalog,
                                       std::unique_ptr<Schema> output_schema,
                                       std::unique_ptr<AbstractOperator> child,
                                       std::vector<std::unique_ptr<AbstractExpression>> expressions)
    : UnaryOperator(catalog, std::move(output_schema), std::move(child))
    , expressions_(std::move(expressions))
{
}

Result<View> ProjectionOperator::execute() const
{
    auto* child = getChild();
    if (!child) {
        return Result<View>::failure("ProjectionOperator requires a child operator");
    }
    auto child_result = child->execute();
    if (!child_result) {
        return child_result; // Propagate error from child
    }
    auto& child_view = child_result.value();

    View view("projection_result");
    ViewTuple dummy_tuple(child_view, 0);
    size_t output_columns = output_schema_->getColumnCount();
    if (expressions_.empty()) {
        // TODO: handle select * case.
        // For now, output all columns not starting with '$'
        for (size_t i = 0; i < output_columns; ++i) {
            const auto& column_info = output_schema_->getColumnInfo(i);
            if (column_info.getName()[0] != '$') {
                view.addColumn(child_view.getColumn(column_info.getName()).viewAs(column_info.getName()));
            }
        }
    } else {
        for (size_t i = 0; i < output_columns; ++i) {
            const auto& expr = expressions_[i];
            const auto& column_info = output_schema_->getColumnInfo(i);
            switch (expr->getExpressionType()) {
            case ExpressionType::COLUMN_REF: {
                const auto* column_ref = static_cast<ColumnRefExpression*>(expr.get());
                view.addColumn(child_view.getColumn(column_ref->getColumnName()).viewAs(column_info.getName()));
                break;
            }
            case ExpressionType::CONSTANT: {
                ValueColumn& constant_col = catalog_.createTemporaryColumn(column_info.getName(),
                                                                           expr->getReturnType().cloneUnique());
                constant_col.fill(expr->evaluate(dummy_tuple, child_view.getSchema()), child_view.getRowCount());
                view.addColumn(constant_col.view());
                break;
            }
            default:
                VELODB_THROW(ExecutionError, "Unsupported expression type in projection: " + expr->toString());
                break;
            }
        }
    }

    return Result<View>::success(std::move(view));
}

} // namespace velodb
