#pragma once

#include "operator/abstract_operator.hpp"

#include <memory>
#include <vector>

namespace velodb {

// Forward declarations
class AbstractExpression;

// Projection operator - handles final materialization and projection
class ProjectionOperator : public UnaryOperator {
public:
    ProjectionOperator(ExecutionContext& context,
                       Schema input_schema,
                       Schema output_schema,
                       std::unique_ptr<AbstractOperator> child,
                       std::vector<std::unique_ptr<AbstractExpression>> expressions);
    ~ProjectionOperator() override = default;

    Result<RowBatch> next() override;

private:
    Schema input_schema_;
    std::vector<std::unique_ptr<AbstractExpression>> expressions_;
};

} // namespace velodb
