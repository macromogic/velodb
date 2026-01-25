#pragma once

#include "expression.hpp"

namespace velodb {

// Comparison expression
class ComparisonExpression : public BinaryExpression {
public:
    ComparisonExpression(ComparisonType comp_type,
                         std::unique_ptr<AbstractExpression> left,
                         std::unique_ptr<AbstractExpression> right);
    ~ComparisonExpression() override = default;

    const Value evaluate(const Tuple& tuple, const Schema& schema) const override;
    Column evaluateBatch(const RowBatch& batch, const Schema& schema) const override;
    std::string toString() const override;

    ComparisonType getComparisonType() const { return comp_type_; }

protected:
    std::unique_ptr<AbstractExpression> cloneUniqueImpl() const override;

private:
    ComparisonType comp_type_;

    Value compareValues(const Value& left_val, const Value& right_val) const;
};

} // namespace velodb
