#include "execution/operator/abstract_operator.hpp"

namespace velodb {

// AbstractOperator implementation
AbstractOperator::AbstractOperator(std::unique_ptr<Schema> output_schema)
    : output_schema_(std::move(output_schema))
{
}

} // namespace velodb
