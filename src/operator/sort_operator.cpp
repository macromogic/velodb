#include "operator/sort_operator.hpp"

#include "expression/expression.hpp"

#include <stdexcept>

namespace velodb {

// SortOperator implementation
SortOperator::SortOperator(ExecutionContext& context,
                           Schema output_schema,
                           std::unique_ptr<AbstractOperator> child,
                           std::vector<std::unique_ptr<AbstractExpression>> sort_expressions,
                           std::vector<bool> ascending_flags)
    : UnaryOperator(context, std::move(output_schema), std::move(child))
    , sort_expressions_(std::move(sort_expressions))
    , ascending_flags_(std::move(ascending_flags))
{
}

Result<RowBatch> SortOperator::next()
{
    // TODO: Implement sorting logic
    return Result<RowBatch>::failure("SortOperator::execute not implemented yet");
}

} // namespace velodb
