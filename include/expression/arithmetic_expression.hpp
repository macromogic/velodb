#pragma once

#include "expression.hpp"

namespace velodb {

// Arithmetic expression
class ArithmeticExpression : public BinaryExpression {
public:
    ArithmeticExpression(ArithmeticType arith_type,
                         std::unique_ptr<DataType> return_type,
                         std::unique_ptr<AbstractExpression> left,
                         std::unique_ptr<AbstractExpression> right);
    ~ArithmeticExpression() override = default;

    const Value evaluate(const Tuple& tuple, const Schema& schema) const override;
    std::string toString() const override;

    ArithmeticType getArithmeticType() const { return arith_type_; }

protected:
    std::unique_ptr<AbstractExpression> cloneUniqueImpl() const override;

private:
    ArithmeticType arith_type_;

    static Value computeArithmetic(const Value& left_val, const Value& right_val, ArithmeticType op_type);

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
