#pragma once

#ifndef VELODB_EXECUTION_EXPRESSION_HPP_INCLUDED
#error "This header should not be included directly. Include execution/expression.hpp instead."
#endif

#include "expression.hpp"

namespace velodb {

// Constant value expression
class ConstantExpression : public AbstractExpression {
public:
    explicit ConstantExpression(const Value& value);
    ~ConstantExpression() override = default;

    Value evaluate(const Tuple* tuple, const Schema* schema) const override;
    [[nodiscard]] bool requiresMaterialization() const override { return false; }
    [[nodiscard]] std::vector<size_t> getRequiredColumns(const Schema& schema) const override;
    [[nodiscard]] std::string toString() const override;

private:
    Value value_;
};

} // namespace velodb
