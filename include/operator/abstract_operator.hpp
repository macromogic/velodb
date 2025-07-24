#pragma once

#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include "catalog/catalog.hpp"
#include "common/non_copyable.hpp"
#include "common/exception.hpp"
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

    // Main execution interface - executes the entire operator tree
    virtual View execute() = 0;

    // Core operator interface - Late materialization only
    virtual void init() = 0;
    virtual bool nextRowId(RowId* row_id) = 0;
    virtual void reset() = 0;

    // Child operator management
    virtual void addChild(std::unique_ptr<AbstractOperator> child);
    const std::vector<std::unique_ptr<AbstractOperator>>& getChildren() const { return children_; }

protected:
    Catalog& catalog_; // Reference to the catalog for table access
    std::unique_ptr<Schema> output_schema_;
    std::vector<std::unique_ptr<AbstractOperator>> children_;
};

} // namespace velodb
