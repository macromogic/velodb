#include "execution/operator/abstract_operator.hpp"

namespace velodb {

// AbstractOperator implementation
AbstractOperator::AbstractOperator(std::unique_ptr<Schema> output_schema)
    : output_schema_(std::move(output_schema))
{
}

void AbstractOperator::addChild(std::unique_ptr<AbstractOperator> child)
{
    children_.push_back(std::move(child));
}

} // namespace velodb
