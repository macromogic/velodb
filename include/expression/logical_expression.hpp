#pragma once

#include "expression.hpp"

#include <vector>

namespace velodb {

class BinaryLogicalExpression : public BinaryExpression {
public:
    BinaryLogicalExpression(ConnectiveType connective_type,
                            std::unique_ptr<AbstractExpression> left,
                            std::unique_ptr<AbstractExpression> right);
    ~BinaryLogicalExpression() override = default;

    const Value evaluate(const Tuple& tuple, const Schema& schema) const override;
    std::string toString() const override;

    ConnectiveType getConnectiveType() const { return connective_type_; }

protected:
    std::unique_ptr<AbstractExpression> cloneUniqueImpl() const override;

private:
    ConnectiveType connective_type_;
};

class LogicalNotExpression : public UnaryExpression {
public:
    explicit LogicalNotExpression(std::unique_ptr<AbstractExpression> operand);
    ~LogicalNotExpression() override = default;

    const Value evaluate(const Tuple& tuple, const Schema& schema) const override;
    std::string toString() const override;

protected:
    std::unique_ptr<AbstractExpression> cloneUniqueImpl() const override;
};

} // namespace velodb
