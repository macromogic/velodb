#pragma once

#include "expression.hpp"

namespace velodb {

// Cast expression for explicit type conversions
class CastExpression : public AbstractExpression {
public:
    CastExpression(std::unique_ptr<AbstractExpression> operand, std::unique_ptr<DataType> target_type);
    ~CastExpression() override = default;

    Value evaluate(const Tuple& tuple, const Schema& schema) const override;
    std::vector<size_t> getRequiredColumns(const Schema& schema) const override;
    std::string toString() const override;

    const DataType& getTargetType() const { return *target_type_; }
    const AbstractExpression& getOperand() const { return *operand_; }

private:
    std::unique_ptr<AbstractExpression> operand_;
    std::unique_ptr<DataType> target_type_;

    // Cast implementation methods
    static Value performCast(const Value& value, const DataType& target_type);
    
    // Type-specific cast methods
    static Value castToBoolean(const Value& value);
    static Value castToInteger(const Value& value);
    static Value castToBigInt(const Value& value);
    static Value castToDouble(const Value& value);
    static Value castToString(const Value& value);
    static Value castToDate(const Value& value);
    static Value castToTimestamp(const Value& value);
};

} // namespace velodb
