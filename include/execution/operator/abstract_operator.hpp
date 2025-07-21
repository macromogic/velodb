#pragma once

#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include "common/non_copyable.hpp"
#include "common/traced_exception.hpp"
#include <memory>
#include <stdexcept>
#include <vector>

namespace velodb {

// Forward declarations
class ExecutionContext;
class QueryResult;

// Abstract base class for all operators
class AbstractOperator : private NonCopyable {
public:
    explicit AbstractOperator(std::unique_ptr<Schema> output_schema);
    virtual ~AbstractOperator() = default;

    [[nodiscard]] const Schema& getOutputSchema() const { return *output_schema_; }

    // Main execution interface - executes the entire operator tree
    virtual std::unique_ptr<QueryResult> execute() = 0;

    // Core operator interface - Late materialization only
    virtual void init() = 0;
    virtual bool nextRowId(RowId* row_id) = 0;
    virtual void reset() = 0;

    // Child operator management
    virtual void addChild(std::unique_ptr<AbstractOperator> child);
    [[nodiscard]] const std::vector<std::unique_ptr<AbstractOperator>>& getChildren() const { return children_; }

    // Legacy interface - deprecated, throws error
    virtual std::vector<Tuple> next() {
        // Legacy interface not supported in late materialization design
        VELODB_THROW(ExecutionError, "Legacy Next() interface not supported - use late materialization only");
    }

protected:
    std::unique_ptr<Schema> output_schema_;
    std::vector<std::unique_ptr<AbstractOperator>> children_;
};

} // namespace velodb
