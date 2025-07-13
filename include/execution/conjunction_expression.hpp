#pragma once

#ifndef VELODB_EXECUTION_EXPRESSION_HPP_INCLUDED
#error "This header should not be included directly. Include execution/expression.hpp instead."
#endif

#include "expression.hpp"
#include <vector>

namespace velodb {

// Conjunction expression (AND/OR)
class ConjunctionExpression : public AbstractExpression {
public:
    ConjunctionExpression(ConjunctionType conj_type,
        std::vector<std::unique_ptr<AbstractExpression>> children);
    ~ConjunctionExpression() override = default;

    Value evaluate(const Tuple* tuple, const Schema* schema) const override;
    Value evaluateJoin(const Tuple* left_tuple, const Schema* left_schema,
        const Tuple* right_tuple, const Schema* right_schema) const override;
    [[nodiscard]] std::vector<size_t> getRequiredColumns(const Schema& schema) const override;
    [[nodiscard]] std::string toString() const override;

    [[nodiscard]] ConjunctionType getConjunctionType() const { return conj_type_; }
    [[nodiscard]] const std::vector<std::unique_ptr<AbstractExpression>>& getChildren() const { return children_; }

private:
    ConjunctionType conj_type_;
    std::vector<std::unique_ptr<AbstractExpression>> children_;
};

} // namespace velodb
