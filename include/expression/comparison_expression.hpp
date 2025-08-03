#pragma once

#include "expression.hpp"

namespace velodb {

// Comparison expression
class ComparisonExpression : public AbstractExpression {
public:
    ComparisonExpression(ComparisonType comp_type,
                         std::unique_ptr<AbstractExpression> left,
                         std::unique_ptr<AbstractExpression> right);
    ~ComparisonExpression() override = default;

    Value evaluate(const Tuple& tuple, const Schema& schema) const override;
    std::vector<size_t> getRequiredColumns(const Schema& schema) const override;
    std::string toString() const override;

    ComparisonType getComparisonType() const { return comp_type_; }
    const AbstractExpression& getLeftExpression() const { return *left_; }
    const AbstractExpression& getRightExpression() const { return *right_; }

private:
    ComparisonType comp_type_;
    std::unique_ptr<AbstractExpression> left_;
    std::unique_ptr<AbstractExpression> right_;

    Value compareValues(const Value& left_val, const Value& right_val) const;
};

} // namespace velodb
