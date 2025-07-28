#pragma once

#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include "types/value.hpp"
#include "common/copy_traits.hpp"
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
    LOGICAL,
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

// Logical connective types
enum class ConnectiveType {
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

    ExpressionType getExpressionType() const { return type_; }
    const DataType& getReturnType() const { return *return_type_; }

    // Core evaluation interface
    virtual Value evaluate(const Tuple& tuple, const Schema& schema) const = 0;

    // Late materialization support
    virtual bool requiresMaterialization() const { return true; }
    virtual std::vector<size_t> getRequiredColumns(const Schema& schema) const = 0;

    virtual std::string toString() const = 0;

protected:
    ExpressionType type_;
    std::unique_ptr<DataType> return_type_;
};

// Forward declarations for concrete expressions
class ConstantExpression;
class ColumnRefExpression;
class ComparisonExpression;
class BinaryLogicalExpression;
class ArithmeticExpression;
class CastExpression;
class FunctionCallExpression;

} // namespace velodb

// Include concrete expression implementations
#include "arithmetic_expression.hpp"
#include "cast_expression.hpp"
#include "column_ref_expression.hpp"
#include "comparison_expression.hpp"
#include "logical_expression.hpp"
#include "constant_expression.hpp"
#include "function_call_expression.hpp"
