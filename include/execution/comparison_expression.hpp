#pragma once

#ifndef VELODB_EXECUTION_EXPRESSION_HPP_INCLUDED
#error "This header should not be included directly. Include execution/expression.hpp instead."
#endif

#include "expression.hpp"

namespace velodb {

// Comparison expression
class ComparisonExpression : public AbstractExpression {
public:
    ComparisonExpression(ComparisonType comp_type,
        std::unique_ptr<AbstractExpression> left,
        std::unique_ptr<AbstractExpression> right);
    ~ComparisonExpression() override = default;

    Value evaluate(const Tuple* tuple, const Schema* schema) const override;
    Value evaluateJoin(const Tuple* left_tuple, const Schema* left_schema,
        const Tuple* right_tuple, const Schema* right_schema) const override;
    [[nodiscard]] std::vector<size_t> getRequiredColumns(const Schema& schema) const override;
    [[nodiscard]] std::string toString() const override;

    [[nodiscard]] ComparisonType getComparisonType() const { return comp_type_; }
    [[nodiscard]] const AbstractExpression& getLeftExpression() const { return *left_; }
    [[nodiscard]] const AbstractExpression& getRightExpression() const { return *right_; }

private:
    ComparisonType comp_type_;
    std::unique_ptr<AbstractExpression> left_;
    std::unique_ptr<AbstractExpression> right_;

    [[nodiscard]] Value compareValues(const Value& left_val, const Value& right_val) const;
};

} // namespace velodb
