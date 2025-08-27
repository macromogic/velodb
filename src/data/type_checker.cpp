#include "data/type_checker.hpp"

#include "common/fmt.hpp"
#include "expression/expression.hpp"

#include <fmt/core.h>

namespace velodb {

// Global type checker instance
TypeChecker g_type_checker;

TypeChecker::TypeChecker()
{
    initializeConversionRules();
}

void TypeChecker::initializeConversionRules()
{
    // Initialize type ranks (higher rank = more general type)
    type_ranks_[DataTypeId::BOOLEAN] = 1;
    type_ranks_[DataTypeId::TINYINT] = 2;
    type_ranks_[DataTypeId::SMALLINT] = 3;
    type_ranks_[DataTypeId::INTEGER] = 4;
    type_ranks_[DataTypeId::BIGINT] = 5;
    type_ranks_[DataTypeId::FLOAT] = 6;
    type_ranks_[DataTypeId::DOUBLE] = 7;
    type_ranks_[DataTypeId::DECIMAL] = 8;
    type_ranks_[DataTypeId::CHAR] = 10;
    type_ranks_[DataTypeId::VARCHAR] = 11;
    type_ranks_[DataTypeId::DATE] = 20;
    type_ranks_[DataTypeId::TIMESTAMP] = 21;

    // Initialize implicit conversion rules (safe widening conversions)
    implicit_conversions_[DataTypeId::TINYINT] = { DataTypeId::SMALLINT, DataTypeId::INTEGER, DataTypeId::BIGINT,
                                                   DataTypeId::FLOAT,    DataTypeId::DOUBLE,  DataTypeId::DECIMAL };
    implicit_conversions_[DataTypeId::SMALLINT]
        = { DataTypeId::INTEGER, DataTypeId::BIGINT, DataTypeId::FLOAT, DataTypeId::DOUBLE, DataTypeId::DECIMAL };
    implicit_conversions_[DataTypeId::INTEGER]
        = { DataTypeId::BIGINT, DataTypeId::FLOAT, DataTypeId::DOUBLE, DataTypeId::DECIMAL };
    implicit_conversions_[DataTypeId::BIGINT] = { DataTypeId::FLOAT, DataTypeId::DOUBLE, DataTypeId::DECIMAL };
    implicit_conversions_[DataTypeId::FLOAT] = { DataTypeId::DOUBLE };
    implicit_conversions_[DataTypeId::CHAR] = { DataTypeId::VARCHAR };
    implicit_conversions_[DataTypeId::DATE] = { DataTypeId::TIMESTAMP };

    // Initialize explicit conversion rules (all valid casts)
    explicit_conversions_[DataTypeId::BOOLEAN]
        = { DataTypeId::TINYINT, DataTypeId::SMALLINT, DataTypeId::INTEGER, DataTypeId::BIGINT, DataTypeId::VARCHAR };
    explicit_conversions_[DataTypeId::TINYINT] = { DataTypeId::BOOLEAN, DataTypeId::SMALLINT, DataTypeId::INTEGER,
                                                   DataTypeId::BIGINT,  DataTypeId::FLOAT,    DataTypeId::DOUBLE,
                                                   DataTypeId::DECIMAL, DataTypeId::VARCHAR };
    explicit_conversions_[DataTypeId::SMALLINT] = { DataTypeId::BOOLEAN, DataTypeId::TINYINT, DataTypeId::INTEGER,
                                                    DataTypeId::BIGINT,  DataTypeId::FLOAT,   DataTypeId::DOUBLE,
                                                    DataTypeId::DECIMAL, DataTypeId::VARCHAR };
    explicit_conversions_[DataTypeId::INTEGER] = { DataTypeId::BOOLEAN, DataTypeId::TINYINT, DataTypeId::SMALLINT,
                                                   DataTypeId::BIGINT,  DataTypeId::FLOAT,   DataTypeId::DOUBLE,
                                                   DataTypeId::DECIMAL, DataTypeId::VARCHAR };
    explicit_conversions_[DataTypeId::BIGINT] = { DataTypeId::BOOLEAN, DataTypeId::TINYINT, DataTypeId::SMALLINT,
                                                  DataTypeId::INTEGER, DataTypeId::FLOAT,   DataTypeId::DOUBLE,
                                                  DataTypeId::DECIMAL, DataTypeId::VARCHAR };
    explicit_conversions_[DataTypeId::FLOAT] = { DataTypeId::BOOLEAN, DataTypeId::TINYINT, DataTypeId::SMALLINT,
                                                 DataTypeId::INTEGER, DataTypeId::BIGINT,  DataTypeId::DOUBLE,
                                                 DataTypeId::DECIMAL, DataTypeId::VARCHAR };
    explicit_conversions_[DataTypeId::DOUBLE] = { DataTypeId::BOOLEAN, DataTypeId::TINYINT, DataTypeId::SMALLINT,
                                                  DataTypeId::INTEGER, DataTypeId::BIGINT,  DataTypeId::FLOAT,
                                                  DataTypeId::DECIMAL, DataTypeId::VARCHAR };
    explicit_conversions_[DataTypeId::DECIMAL] = { DataTypeId::BOOLEAN, DataTypeId::TINYINT, DataTypeId::SMALLINT,
                                                   DataTypeId::INTEGER, DataTypeId::BIGINT,  DataTypeId::FLOAT,
                                                   DataTypeId::DOUBLE,  DataTypeId::VARCHAR };
    explicit_conversions_[DataTypeId::CHAR] = { DataTypeId::VARCHAR,  DataTypeId::BOOLEAN,  DataTypeId::TINYINT,
                                                DataTypeId::SMALLINT, DataTypeId::INTEGER,  DataTypeId::BIGINT,
                                                DataTypeId::FLOAT,    DataTypeId::DOUBLE,   DataTypeId::DECIMAL,
                                                DataTypeId::DATE,     DataTypeId::TIMESTAMP };
    explicit_conversions_[DataTypeId::VARCHAR] = { DataTypeId::CHAR,     DataTypeId::BOOLEAN,  DataTypeId::TINYINT,
                                                   DataTypeId::SMALLINT, DataTypeId::INTEGER,  DataTypeId::BIGINT,
                                                   DataTypeId::FLOAT,    DataTypeId::DOUBLE,   DataTypeId::DECIMAL,
                                                   DataTypeId::DATE,     DataTypeId::TIMESTAMP };
    explicit_conversions_[DataTypeId::DATE] = { DataTypeId::VARCHAR, DataTypeId::TIMESTAMP };
    explicit_conversions_[DataTypeId::TIMESTAMP] = { DataTypeId::VARCHAR, DataTypeId::DATE };
}

bool TypeChecker::validateExpression(const AbstractExpression* expr) const
{
    if (!expr) {
        setError("Expression is null");
        return false;
    }

    // TODO: Implement comprehensive expression validation
    // This would involve traversing the expression tree and validating each
    // node
    return true;
}

std::unique_ptr<DataType> TypeChecker::deduceArithmeticType(const DataType& left_type,
                                                            const DataType& right_type,
                                                            ArithmeticType op_type) const
{

    // Validate that both operands are numeric
    if (!validateArithmeticOperands(left_type, right_type, op_type)) {
        return nullptr;
    }

    // For division, always promote to DOUBLE to handle fractional results
    if (op_type == ArithmeticType::DIVIDE) {
        return std::make_unique<DoubleType>();
    }

    // For modulo, keep integer types
    if (op_type == ArithmeticType::MODULO) {
        if (left_type.getTypeId() == DataTypeId::BIGINT || right_type.getTypeId() == DataTypeId::BIGINT) {
            return std::make_unique<BigIntType>();
        }
        return std::make_unique<IntegerType>();
    }

    // For other operations, use type promotion
    return promoteTypes(left_type, right_type);
}

bool TypeChecker::validateComparison(const DataType& left_type,
                                     const DataType& right_type,
                                     ComparisonType comp_type) const
{

    return validateComparisonOperands(left_type, right_type, comp_type);
}

ConversionResult TypeChecker::canConvert(const DataType& from_type, const DataType& to_type) const
{
    DataTypeId from_id = from_type.getTypeId();
    DataTypeId to_id = to_type.getTypeId();

    // Same type is always valid
    if (from_id == to_id) {
        return ConversionResult::VALID;
    }

    // Check implicit conversions (safe)
    auto implicit_it = implicit_conversions_.find(from_id);
    if (implicit_it != implicit_conversions_.end() && implicit_it->second.count(to_id) > 0) {
        return ConversionResult::VALID;
    }

    // Check explicit conversions
    auto explicit_it = explicit_conversions_.find(from_id);
    if (explicit_it != explicit_conversions_.end() && explicit_it->second.count(to_id) > 0) {

        // Determine if conversion may lose precision
        if (isNumericType(from_type) && isNumericType(to_type)) {
            int from_rank = getTypeRank(from_type);
            int to_rank = getTypeRank(to_type);
            if (from_rank > to_rank) {
                return ConversionResult::VALID_WITH_LOSS;
            }
        }

        // String to numeric conversions need runtime validation
        if (isStringType(from_type) && isNumericType(to_type)) {
            return ConversionResult::RUNTIME_CHECK;
        }

        return ConversionResult::VALID;
    }

    return ConversionResult::INVALID;
}

bool TypeChecker::canImplicitlyConvert(const DataType& from_type, const DataType& to_type) const
{
    ConversionResult result = canConvert(from_type, to_type);
    return result == ConversionResult::VALID;
}

std::unique_ptr<DataType> TypeChecker::promoteTypes(const DataType& left_type, const DataType& right_type) const
{
    DataTypeId left_id = left_type.getTypeId();
    DataTypeId right_id = right_type.getTypeId();

    // If types are the same, return that type
    if (left_id == right_id) {
        return DataType::createType(left_id, std::max(left_type.size(), right_type.size()));
    }

    // Promote to the higher-ranked type
    int left_rank = getTypeRank(left_type);
    int right_rank = getTypeRank(right_type);

    if (left_rank >= right_rank) {
        return DataType::createType(left_id, left_type.size());
    } else {
        return DataType::createType(right_id, right_type.size());
    }
}

bool TypeChecker::validateCast(const DataType& from_type, const DataType& to_type) const
{
    ConversionResult result = canConvert(from_type, to_type);
    return result != ConversionResult::INVALID;
}

bool TypeChecker::isNumericType(const DataType& type) const
{
    return type.isNumeric();
}

bool TypeChecker::isStringType(const DataType& type) const
{
    DataTypeId type_id = type.getTypeId();
    return type_id == DataTypeId::CHAR || type_id == DataTypeId::VARCHAR;
}

bool TypeChecker::isDateTimeType(const DataType& type) const
{
    DataTypeId type_id = type.getTypeId();
    return type_id == DataTypeId::DATE || type_id == DataTypeId::TIMESTAMP;
}

int TypeChecker::getTypeRank(const DataType& type) const
{
    auto it = type_ranks_.find(type.getTypeId());
    return (it != type_ranks_.end()) ? it->second : 0;
}

void TypeChecker::setError(const std::string& error) const
{
    last_error_ = error;
}

bool TypeChecker::validateArithmeticOperands(const DataType& left_type,
                                             const DataType& right_type,
                                             ArithmeticType op_type) const
{
    // All arithmetic operations require numeric operands
    if (!isNumericType(left_type)) {
        setError(fmt::format("Left operand of arithmetic operation must be numeric, got {}", left_type));
        return false;
    }

    if (!isNumericType(right_type)) {
        setError(fmt::format("Right operand of arithmetic operation must be numeric, got {}", right_type));
        return false;
    }

    // Modulo operation has additional restrictions
    if (op_type == ArithmeticType::MODULO) {
        // Both operands should be integer types for modulo
        DataTypeId left_id = left_type.getTypeId();
        DataTypeId right_id = right_type.getTypeId();

        bool left_is_int = (left_id == DataTypeId::TINYINT || left_id == DataTypeId::SMALLINT
                            || left_id == DataTypeId::INTEGER || left_id == DataTypeId::BIGINT);
        bool right_is_int = (right_id == DataTypeId::TINYINT || right_id == DataTypeId::SMALLINT
                             || right_id == DataTypeId::INTEGER || right_id == DataTypeId::BIGINT);

        if (!left_is_int || !right_is_int) {
            setError("Modulo operation requires integer operands");
            return false;
        }
    }

    return true;
}

bool TypeChecker::validateComparisonOperands(const DataType& left_type,
                                             const DataType& right_type,
                                             ComparisonType comp_type) const
{
    DataTypeId left_id = left_type.getTypeId();
    DataTypeId right_id = right_type.getTypeId();

    // NULL comparisons are always valid
    if (comp_type == ComparisonType::IS_NULL || comp_type == ComparisonType::IS_NOT_NULL) {
        return true;
    }

    // Same types are always comparable
    if (left_id == right_id) {
        return true;
    }

    // Check if types can be promoted for comparison
    if (isNumericType(left_type) && isNumericType(right_type)) {
        return true; // All numeric types are comparable
    }

    if (isStringType(left_type) && isStringType(right_type)) {
        return true; // All string types are comparable
    }

    if (isDateTimeType(left_type) && isDateTimeType(right_type)) {
        return true; // Date/time types are comparable
    }

    // LIKE operations require string operands
    if (comp_type == ComparisonType::LIKE || comp_type == ComparisonType::NOT_LIKE) {
        if (!isStringType(left_type) || !isStringType(right_type)) {
            setError("LIKE operation requires string operands");
            return false;
        }
        return true;
    }

    // Check if implicit conversion is possible
    if (canImplicitlyConvert(left_type, right_type) || canImplicitlyConvert(right_type, left_type)) {
        return true;
    }

    setError(fmt::format("Cannot compare {} with {}", left_type, right_type));
    return false;
}

} // namespace velodb
