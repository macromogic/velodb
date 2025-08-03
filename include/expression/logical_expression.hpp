#pragma once

#include "expression.hpp"

#include <vector>

namespace velodb {

// Logical expression (AND/OR)
class BinaryLogicalExpression : public AbstractExpression {
public:
    BinaryLogicalExpression(ConnectiveType connective_type,
                            std::unique_ptr<AbstractExpression> left,
                            std::unique_ptr<AbstractExpression> right);
    ~BinaryLogicalExpression() override = default;

    Value evaluate(const Tuple& tuple, const Schema& schema) const override;
    std::vector<size_t> getRequiredColumns(const Schema& schema) const override;
    std::string toString() const override;

    ConnectiveType getConjunctionType() const { return connective_type_; }

private:
    ConnectiveType connective_type_;
    std::unique_ptr<AbstractExpression> left_;
    std::unique_ptr<AbstractExpression> right_;
};

class LogicalNotExpression : public AbstractExpression {
public:
    explicit LogicalNotExpression(std::unique_ptr<AbstractExpression> operand);
    ~LogicalNotExpression() override = default;

    Value evaluate(const Tuple& tuple, const Schema& schema) const override;
    std::vector<size_t> getRequiredColumns(const Schema& schema) const override;
    std::string toString() const override;

private:
    std::unique_ptr<AbstractExpression> operand_;
};

} // namespace velodb
