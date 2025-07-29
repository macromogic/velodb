#include "operator/abstract_operator.hpp"
#include "catalog/catalog.hpp"

namespace velodb {

// AbstractOperator implementation
AbstractOperator::AbstractOperator(Catalog& catalog, std::unique_ptr<Schema> output_schema)
    : catalog_(catalog)
    , output_schema_(std::move(output_schema))
{
}

UnaryOperator::UnaryOperator(Catalog& catalog, std::unique_ptr<Schema> output_schema, std::unique_ptr<AbstractOperator> child)
    : AbstractOperator(catalog, std::move(output_schema))
    , child_(std::move(child))
{
}

BinaryOperator::BinaryOperator(Catalog& catalog,
    std::unique_ptr<Schema> output_schema,
    std::unique_ptr<AbstractOperator> left_child,
    std::unique_ptr<AbstractOperator> right_child)
    : AbstractOperator(catalog, std::move(output_schema))
    , left_child_(std::move(left_child))
    , right_child_(std::move(right_child))
{
}

} // namespace velodb
