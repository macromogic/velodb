#pragma once

#include "expression.hpp"
#include <vector>

namespace velodb {

// Conjunction expression (AND/OR)
class ConjunctionExpression : public AbstractExpression {
public:
    ConjunctionExpression(ConjunctionType conj_type,
        std::unique_ptr<AbstractExpression> left,
        std::unique_ptr<AbstractExpression> right);
    ~ConjunctionExpression() override = default;

    Value evaluate(const Tuple& tuple, const Schema& schema) const override;
    std::vector<size_t> getRequiredColumns(const Schema& schema) const override;
    std::string toString() const override;

    ConjunctionType getConjunctionType() const { return conj_type_; }

private:
    ConjunctionType conj_type_;
    std::unique_ptr<AbstractExpression> left_;
    std::unique_ptr<AbstractExpression> right_;
};

} // namespace velodb
