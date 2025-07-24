#pragma once

#include "expression.hpp"
#include <vector>

namespace velodb {

// Conjunction expression (AND/OR)
class ConjunctionExpression : public AbstractExpression {
public:
    ConjunctionExpression(ConjunctionType conj_type,
        std::vector<std::unique_ptr<AbstractExpression>> children);
    ~ConjunctionExpression() override = default;

    Value evaluate(const Tuple& tuple, const Schema& schema) const override;
    std::vector<size_t> getRequiredColumns(const Schema& schema) const override;
    std::string toString() const override;

    ConjunctionType getConjunctionType() const { return conj_type_; }
    const std::vector<std::unique_ptr<AbstractExpression>>& getChildren() const { return children_; }

private:
    ConjunctionType conj_type_;
    std::vector<std::unique_ptr<AbstractExpression>> children_;
};

} // namespace velodb
