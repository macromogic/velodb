#pragma once

#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include <memory>
#include <stdexcept>
#include <vector>

namespace velodb {

// Forward declarations
class ExecutionContext;

// Abstract base class for all operators
class AbstractOperator {
public:
    explicit AbstractOperator(std::unique_ptr<Schema> output_schema);
    virtual ~AbstractOperator() = default;

    // Delete copy constructor and assignment
    AbstractOperator(const AbstractOperator&) = delete;
    AbstractOperator& operator=(const AbstractOperator&) = delete;

    [[nodiscard]] const Schema& getOutputSchema() const { return *output_schema_; }

    // Core operator interface - Late materialization only
    virtual void init() = 0;
    virtual bool nextRowId(RowId* row_id) = 0;
    virtual void reset() = 0;

    // Materialization interface
    virtual void materializeRowIds(const std::vector<RowId>& row_ids,
        const std::vector<size_t>& column_indices,
        std::vector<Tuple>* tuples)
        = 0;

    // Legacy interface - deprecated, throws error
    virtual bool next([[maybe_unused]] Tuple* tuple, [[maybe_unused]] RowId* row_id)
    {
        throw std::runtime_error("Legacy Next() interface not supported - use late materialization only");
    }

protected:
    std::unique_ptr<Schema> output_schema_;
};

} // namespace velodb
