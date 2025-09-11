#include "operator/limit_operator.hpp"

namespace velodb {

// LimitOperator implementation
LimitOperator::LimitOperator(ExecutionContext& context,
                             Schema output_schema,
                             std::unique_ptr<AbstractOperator> child,
                             size_t limit,
                             size_t offset)
    : UnaryOperator(context, std::move(output_schema), std::move(child))
    , limit_(limit)
    , offset_(offset)
{
}

Result<RowBatch> LimitOperator::next()
{
    // TODO: Implement limit and offset logic
    return Result<RowBatch>::failure("LimitOperator::execute not implemented yet");
}

} // namespace velodb
