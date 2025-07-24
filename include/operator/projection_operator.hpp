#pragma once

#include "operator/abstract_operator.hpp"
#include <memory>
#include <vector>

namespace velodb {

// Forward declarations
class AbstractExpression;
class View;
class TableBase;

// Projection operator - handles final materialization and projection
class ProjectionOperator : public UnaryOperator {
public:
    ProjectionOperator(Catalog& catalog,
        std::unique_ptr<Schema> output_schema,
        std::unique_ptr<AbstractOperator> child,
        std::vector<std::unique_ptr<AbstractExpression>> expressions);
    ~ProjectionOperator() override = default;

    Result<View> execute() const override;

private:
    std::vector<std::unique_ptr<AbstractExpression>> expressions_;
};

} // namespace velodb
