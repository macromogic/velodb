#pragma once

#define VELODB_EXECUTION_EXPRESSION_HPP_INCLUDED

#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include "types/value.hpp"
#include "common/non_copyable.hpp"
#include <memory>
#include <string>
#include <vector>

namespace velodb {

// Forward declarations
class ExecutionContext;

// Expression types
enum class ExpressionType {
    INVALID = 0,
    CONSTANT,
    COLUMN_REF,
    COMPARISON,
    CONJUNCTION,
    ARITHMETIC,
    FUNCTION_CALL,
    CAST,
    CASE,
    SUBQUERY
};

// Comparison types
enum class ComparisonType {
    EQUAL,
    NOT_EQUAL,
    LESS_THAN,
    LESS_THAN_OR_EQUAL,
    GREATER_THAN,
    GREATER_THAN_OR_EQUAL,
    LIKE,
    NOT_LIKE,
    IN,
    NOT_IN,
    IS_NULL,
    IS_NOT_NULL
};

// Conjunction types
enum class ConjunctionType {
    AND,
    OR
};

// Arithmetic types
enum class ArithmeticType {
    PLUS,
    MINUS,
    MULTIPLY,
    DIVIDE,
    MODULO
};

// Abstract base class for all expressions
class AbstractExpression : private NonCopyable {
public:
    AbstractExpression(ExpressionType type, std::unique_ptr<DataType> return_type);
    virtual ~AbstractExpression() = default;

    [[nodiscard]] ExpressionType getExpressionType() const { return type_; }
    [[nodiscard]] const DataType& getReturnType() const { return *return_type_; }

    // Core evaluation interface
    virtual Value evaluate(const Tuple* tuple, const Schema* schema) const = 0;
    virtual Value evaluateJoin(const Tuple* left_tuple, const Schema* left_schema,
        const Tuple* right_tuple, const Schema* right_schema) const;

    // Late materialization support
    [[nodiscard]] virtual bool requiresMaterialization() const { return true; }
    [[nodiscard]] virtual std::vector<size_t> getRequiredColumns(const Schema& schema) const = 0;

    [[nodiscard]] virtual std::string toString() const = 0;

protected:
    ExpressionType type_;
    std::unique_ptr<DataType> return_type_;
};

// Forward declarations for concrete expressions
class ConstantExpression;
class ColumnRefExpression;
class ComparisonExpression;
class ConjunctionExpression;
class ArithmeticExpression;
class CastExpression;
class FunctionCallExpression;

} // namespace velodb

// Include concrete expression implementations
#include "arithmetic_expression.hpp"
#include "cast_expression.hpp"
#include "column_ref_expression.hpp"
#include "comparison_expression.hpp"
#include "conjunction_expression.hpp"
#include "constant_expression.hpp"
#include "function_call_expression.hpp"
