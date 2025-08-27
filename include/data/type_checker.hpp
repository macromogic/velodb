#pragma once

#include "data/data_type.hpp"
#include "data/value.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace velodb {

// Forward declarations
class AbstractExpression;
enum class ArithmeticType;
enum class ComparisonType;

// Type conversion result
enum class ConversionResult {
    VALID, // Conversion is valid and safe
    VALID_WITH_LOSS, // Conversion is valid but may lose precision
    INVALID, // Conversion is not allowed
    RUNTIME_CHECK // Conversion validity depends on runtime value
};

// Type compatibility for operations
enum class TypeCompatibility {
    COMPATIBLE, // Types are directly compatible
    PROMOTABLE, // Types can be promoted to compatible types
    CASTABLE, // Types require explicit cast
    INCOMPATIBLE // Types cannot be used together
};

class TypeChecker {
public:
    TypeChecker();
    ~TypeChecker() = default;

    // Type validation for expressions
    bool validateExpression(const AbstractExpression* expr) const;
    std::string getLastError() const { return last_error_; }

    // Arithmetic type checking
    std::unique_ptr<DataType> deduceArithmeticType(const DataType& left_type,
                                                   const DataType& right_type,
                                                   ArithmeticType op_type) const;

    // Comparison type checking
    bool validateComparison(const DataType& left_type, const DataType& right_type, ComparisonType comp_type) const;

    // Type conversion checking
    ConversionResult canConvert(const DataType& from_type, const DataType& to_type) const;
    bool canImplicitlyConvert(const DataType& from_type, const DataType& to_type) const;

    // Type promotion
    std::unique_ptr<DataType> promoteTypes(const DataType& left_type, const DataType& right_type) const;

    // Cast validation
    bool validateCast(const DataType& from_type, const DataType& to_type) const;

    // Utility methods
    bool isNumericType(const DataType& type) const;
    bool isStringType(const DataType& type) const;
    bool isDateTimeType(const DataType& type) const;
    int getTypeRank(const DataType& type) const;

private:
    mutable std::string last_error_;

    // Type conversion matrices
    std::unordered_map<DataTypeId, std::unordered_set<DataTypeId>> implicit_conversions_;
    std::unordered_map<DataTypeId, std::unordered_set<DataTypeId>> explicit_conversions_;
    std::unordered_map<DataTypeId, int> type_ranks_;

    void initializeConversionRules();
    void setError(const std::string& error) const;

    // Helper methods for specific type checks
    bool validateArithmeticOperands(const DataType& left_type,
                                    const DataType& right_type,
                                    ArithmeticType op_type) const;
    bool validateComparisonOperands(const DataType& left_type,
                                    const DataType& right_type,
                                    ComparisonType comp_type) const;
};

// Global type checker instance
extern TypeChecker g_type_checker;

} // namespace velodb
