#pragma once

#include "expression.hpp"

namespace velodb {

// Constant value expression
class ConstantExpression : public AbstractExpression {
public:
    explicit ConstantExpression(const Value& value);
    ~ConstantExpression() override = default;

    Value evaluate(const Tuple& tuple, const Schema& schema) const override;
    bool requiresMaterialization() const override { return false; }
    std::vector<size_t> getRequiredColumns(const Schema& schema) const override;
    std::string toString() const override;

private:
    Value value_;
};

} // namespace velodb
