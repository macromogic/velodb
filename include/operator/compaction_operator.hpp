#pragma once

#include "operator/abstract_operator.hpp"
#include <memory>

namespace velodb {

// Forward declarations
class View;

class CompactionOperator : public UnaryOperator {
public:
    CompactionOperator(Catalog& catalog,
        std::unique_ptr<Schema> output_schema,
        std::unique_ptr<AbstractOperator> child);
    ~CompactionOperator() override = default;
    Result<View> execute() const override;
};

} // namespace velodb
