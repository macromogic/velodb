#pragma once

#include "catalog/execution_context.hpp"
#include "catalog/row_batch.hpp"
#include "catalog/schema.hpp"
#include "common/copy_traits.hpp"
#include "common/profiler.hpp"
#include "common/result.hpp"

#include <memory>

namespace velodb {

// Forward declarations
class RowBatch;

// Abstract base class for all operators
class AbstractOperator : private NonCopyable {
public:
    explicit AbstractOperator(ExecutionContext& context, Schema output_schema);
    virtual ~AbstractOperator() = default;

    const Schema& getOutputSchema() const { return output_schema_; }

    virtual Result<RowBatch> next() = 0;
    virtual bool isUnary() const = 0;

    static constexpr size_t MAX_BATCH_SIZE = 1ul << 20; // 1M rows

protected:
    ExecutionContext& context_;
    Schema output_schema_;
};

class UnaryOperator : public AbstractOperator {
public:
    explicit UnaryOperator(ExecutionContext& context, Schema output_schema, std::unique_ptr<AbstractOperator> child);

    bool isUnary() const override { return true; }

    AbstractOperator* getChild() const { return child_.get(); }

private:
    std::unique_ptr<AbstractOperator> child_; // Child operator
};

class BinaryOperator : public AbstractOperator {
public:
    BinaryOperator(ExecutionContext& context,
                   Schema output_schema,
                   std::unique_ptr<AbstractOperator> left_child,
                   std::unique_ptr<AbstractOperator> right_child);

    bool isUnary() const override { return false; }

    AbstractOperator* getLeftChild() const { return left_child_.get(); }
    AbstractOperator* getRightChild() const { return right_child_.get(); }

private:
    std::unique_ptr<AbstractOperator> left_child_;
    std::unique_ptr<AbstractOperator> right_child_;
};

} // namespace velodb
