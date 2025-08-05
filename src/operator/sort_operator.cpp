#include "operator/sort_operator.hpp"

#include "expression/expression.hpp"

#include <stdexcept>

namespace velodb {

// SortOperator implementation
SortOperator::SortOperator(ExecutionContext& context,
                           std::unique_ptr<Schema> output_schema,
                           std::unique_ptr<AbstractOperator> child,
                           std::vector<std::unique_ptr<AbstractExpression>> sort_expressions,
                           std::vector<bool> ascending_flags)
    : UnaryOperator(context, std::move(output_schema), std::move(child))
    , sort_expressions_(std::move(sort_expressions))
    , ascending_flags_(std::move(ascending_flags))
{
}

Result<View> SortOperator::next() const
{
    // TODO: Implement sorting logic
    return Result<View>::failure("SortOperator::execute not implemented yet");
}

} // namespace velodb
