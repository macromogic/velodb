#pragma once

#include "catalog/catalog.hpp"
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
    explicit AbstractOperator(Catalog& catalog, std::unique_ptr<Schema> output_schema);
    virtual ~AbstractOperator() = default;

    const Schema& getOutputSchema() const { return *output_schema_; }

    virtual Result<View> execute() const = 0;
    virtual bool isUnary() const = 0;

protected:
    Catalog& catalog_; // Reference to the catalog for table access
    std::unique_ptr<Schema> output_schema_;
};

class UnaryOperator : public AbstractOperator {
public:
    explicit UnaryOperator(Catalog& catalog,
                           std::unique_ptr<Schema> output_schema,
                           std::unique_ptr<AbstractOperator> child);

    bool isUnary() const override { return true; }

    const AbstractOperator* getChild() const { return child_.get(); }

private:
    std::unique_ptr<AbstractOperator> child_; // Child operator
};

class BinaryOperator : public AbstractOperator {
public:
    BinaryOperator(Catalog& catalog,
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
