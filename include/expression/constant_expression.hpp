#pragma once

#include "expression.hpp"

namespace velodb {

// Constant value expression
class ConstantExpression : public LeafExpression {
public:
    explicit ConstantExpression(const Value& value);
    ~ConstantExpression() override = default;

    const Value evaluate(const Tuple& tuple, const Schema& schema) const override;
    const Value getValue() const;
    std::string toString() const override;

protected:
    std::unique_ptr<AbstractExpression> cloneUniqueImpl() const override;

private:
    Value value_;
};

} // namespace velodb
