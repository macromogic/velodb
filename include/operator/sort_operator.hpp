#pragma once

#include "operator/abstract_operator.hpp"
#include <memory>
#include <vector>

namespace velodb {

// Forward declarations
class AbstractExpression;

// Sort operator
class SortOperator : public UnaryOperator {
public:
    SortOperator(Catalog& catalog,
        std::unique_ptr<Schema> output_schema,
        std::unique_ptr<AbstractOperator> child,
        std::vector<std::unique_ptr<AbstractExpression>> sort_expressions,
        std::vector<bool> ascending_flags);
    ~SortOperator() override = default;

    Result<View> execute() const override;

private:
    std::vector<std::unique_ptr<AbstractExpression>> sort_expressions_;
    std::vector<bool> ascending_flags_;
};

} // namespace velodb
