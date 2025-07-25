#include "operator/limit_operator.hpp"

namespace velodb {

// LimitOperator implementation
LimitOperator::LimitOperator(Catalog& catalog,
        std::unique_ptr<Schema> output_schema,
        std::unique_ptr<AbstractOperator> child,
        size_t limit,
        size_t offset)
    : UnaryOperator(catalog, std::move(output_schema), std::move(child))
    , limit_(limit)
    , offset_(offset)
{
}

Result<View> LimitOperator::execute() const
{
    // TODO: Implement limit and offset logic
    return Result<View>::failure("LimitOperator::execute not implemented yet");
}

} // namespace velodb
