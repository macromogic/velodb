#pragma once

#include "catalog/execution_context.hpp"
#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include "common/copy_traits.hpp"
#include "common/exception.hpp"
#include "common/result.hpp"

#include <memory>
#include <stdexcept>
#include <vector>

namespace velodb {

// Forward declarations
class ExecutionContext;
class View;

// Abstract base class for all operators
class AbstractOperator : private NonCopyable {
public:
    explicit AbstractOperator(ExecutionContext& context, std::unique_ptr<Schema> output_schema);
    virtual ~AbstractOperator() = default;

    const Schema& getOutputSchema() const { return *output_schema_; }

    virtual Result<View> next() const = 0;
    virtual bool isUnary() const = 0;

    static constexpr size_t MAX_BATCH_SIZE = 32;

protected:
    ExecutionContext& context_;
    std::unique_ptr<Schema> output_schema_;
};

class UnaryOperator : public AbstractOperator {
public:
    explicit UnaryOperator(ExecutionContext& context,
                           std::unique_ptr<Schema> output_schema,
                           std::unique_ptr<AbstractOperator> child);

    bool isUnary() const override { return true; }

    const AbstractOperator* getChild() const { return child_.get(); }

private:
    std::unique_ptr<AbstractOperator> child_; // Child operator
};

class BinaryOperator : public AbstractOperator {
public:
    BinaryOperator(ExecutionContext& context,
                   std::unique_ptr<Schema> output_schema,
                   std::unique_ptr<AbstractOperator> left_child,
                   std::unique_ptr<AbstractOperator> right_child);

    bool isUnary() const override { return false; }

    const AbstractOperator* getLeftChild() const { return left_child_.get(); }
    const AbstractOperator* getRightChild() const { return right_child_.get(); }

private:
    std::unique_ptr<AbstractOperator> left_child_;
    std::unique_ptr<AbstractOperator> right_child_;
};

} // namespace velodb
