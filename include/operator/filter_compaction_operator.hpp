#pragma once

#include "operator/abstract_operator.hpp"

#include <memory>

namespace velodb {

// Forward declarations
class View;

class FilterCompactionOperator : public UnaryOperator {
public:
    FilterCompactionOperator(ExecutionContext& context,
                             std::unique_ptr<Schema> output_schema,
                             std::unique_ptr<AbstractOperator> child);
    ~FilterCompactionOperator() override = default;
    Result<View> next() override;
};

} // namespace velodb
