#include "operator/abstract_operator.hpp"

#include "catalog/execution_context.hpp"

namespace velodb {

// AbstractOperator implementation
AbstractOperator::AbstractOperator(ExecutionContext& context, Schema output_schema)
    : context_(context)
    , output_schema_(std::move(output_schema))
{
}

UnaryOperator::UnaryOperator(ExecutionContext& context, Schema output_schema, std::unique_ptr<AbstractOperator> child)
    : AbstractOperator(context, std::move(output_schema))
    , child_(std::move(child))
{
}

BinaryOperator::BinaryOperator(ExecutionContext& context,
                               Schema output_schema,
                               std::unique_ptr<AbstractOperator> left_child,
                               std::unique_ptr<AbstractOperator> right_child)
    : AbstractOperator(context, std::move(output_schema))
    , left_child_(std::move(left_child))
    , right_child_(std::move(right_child))
{
}

} // namespace velodb
