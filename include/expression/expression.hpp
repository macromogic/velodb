#pragma once

#include "common/copy_traits.hpp"
#include "data/value.hpp"

#include <memory>
#include <string>
#include <vector>

namespace velodb {

// Forward declarations
class ExecutionContext;
class Tuple;
class Schema;
class RowBatch;
class Column;

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
class AbstractExpression : private NonCopyable, public UniqueCloneable<AbstractExpression> {
public:
    AbstractExpression(ExpressionType type, std::unique_ptr<DataType> return_type);
    AbstractExpression(AbstractExpression&&) noexcept = default;
    AbstractExpression& operator=(AbstractExpression&&) noexcept = default;
    virtual ~AbstractExpression() = default;

    ExpressionType getExpressionType() const { return type_; }
    const DataType& getReturnType() const { return *return_type_; }

    virtual bool isLeaf() const { return false; }
    virtual bool isUnary() const { return false; }
    virtual bool isBinary() const { return false; }

    // Core evaluation interface
    virtual const Value evaluate(const Tuple& tuple, const Schema& schema) const = 0;

    // Vectorized evaluation interface
    virtual Column evaluateBatch(const RowBatch& batch, const Schema& schema) const;

    virtual std::string toString() const = 0;

    void setDebugFlag(bool flag) { debug_flag_ = flag; }

protected:
    virtual std::unique_ptr<AbstractExpression> cloneUniqueImpl() const = 0;
    friend class UniqueCloneable<AbstractExpression>;

    void setSizeForColumn(Column& col, size_t size) const;

    ExpressionType type_;
    std::unique_ptr<DataType> return_type_;
    static bool debug_flag_;
};

class LeafExpression : public AbstractExpression {
public:
    LeafExpression(ExpressionType type, std::unique_ptr<DataType> return_type);
    LeafExpression(LeafExpression&&) noexcept = default;
    LeafExpression& operator=(LeafExpression&&) noexcept = default;

    bool isLeaf() const override { return true; }
};

class UnaryExpression : public AbstractExpression {
public:
    UnaryExpression(ExpressionType type,
                    std::unique_ptr<DataType> return_type,
                    std::unique_ptr<AbstractExpression> operand);
    UnaryExpression(UnaryExpression&&) noexcept = default;
    UnaryExpression& operator=(UnaryExpression&&) noexcept = default;

    const AbstractExpression& getOperandExpression() const { return *operand_; }

    bool isUnary() const override { return true; }

protected:
    std::unique_ptr<AbstractExpression> operand_;
};

class BinaryExpression : public AbstractExpression {
public:
    BinaryExpression(ExpressionType type,
                     std::unique_ptr<DataType> return_type,
                     std::unique_ptr<AbstractExpression> left,
                     std::unique_ptr<AbstractExpression> right);
    BinaryExpression(BinaryExpression&&) noexcept = default;
    BinaryExpression& operator=(BinaryExpression&&) noexcept = default;

    const AbstractExpression& getLeftExpression() const { return *left_; }
    const AbstractExpression& getRightExpression() const { return *right_; }

    bool isBinary() const override { return true; }

protected:
    std::unique_ptr<AbstractExpression> left_;
    std::unique_ptr<AbstractExpression> right_;
};

} // namespace velodb
