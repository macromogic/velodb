#pragma once

#ifndef VELODB_EXECUTION_EXPRESSION_HPP_INCLUDED
#error "This header should not be included directly. Include execution/expression.hpp instead."
#endif

#include "expression.hpp"

namespace velodb {

// Arithmetic expression
class ArithmeticExpression : public AbstractExpression {
public:
    ArithmeticExpression(ArithmeticType arith_type,
        std::unique_ptr<AbstractExpression> left,
        std::unique_ptr<AbstractExpression> right);
    ~ArithmeticExpression() override = default;

    Value evaluate(const Tuple* tuple, const Schema* schema) const override;
    Value evaluateJoin(const Tuple* left_tuple, const Schema* left_schema,
        const Tuple* right_tuple, const Schema* right_schema) const override;
    [[nodiscard]] std::vector<size_t> getRequiredColumns(const Schema& schema) const override;
    [[nodiscard]] std::string toString() const override;

    [[nodiscard]] ArithmeticType getArithmeticType() const { return arith_type_; }

private:
    ArithmeticType arith_type_;
    std::unique_ptr<AbstractExpression> left_;
    std::unique_ptr<AbstractExpression> right_;

    [[nodiscard]] static Value computeArithmetic(const Value& left_val, const Value& right_val, ArithmeticType op_type);

private:
    // Helper methods for type conversion
    static int32_t convertToInteger(const Value& val);
    static int64_t convertToBigInt(const Value& val);
    static double convertToDouble(const Value& val);
    
    // Helper methods for arithmetic operations
    static int32_t performIntegerArithmetic(int32_t left, int32_t right, ArithmeticType op_type);
    static int64_t performBigIntArithmetic(int64_t left, int64_t right, ArithmeticType op_type);
    static double performDoubleArithmetic(double left, double right, ArithmeticType op_type);
};

} // namespace velodb
