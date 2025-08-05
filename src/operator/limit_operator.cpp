#include "operator/limit_operator.hpp"

namespace velodb {

// LimitOperator implementation
LimitOperator::LimitOperator(ExecutionContext& context,
                             std::unique_ptr<Schema> output_schema,
                             std::unique_ptr<AbstractOperator> child,
                             size_t limit,
                             size_t offset)
    : UnaryOperator(context, std::move(output_schema), std::move(child))
    , limit_(limit)
    , offset_(offset)
{
}

Result<View> LimitOperator::next() const
{
    // TODO: Implement limit and offset logic
    return Result<View>::failure("LimitOperator::execute not implemented yet");
}

} // namespace velodb
