#pragma once

#include "operator/abstract_operator.hpp"
#include <memory>

namespace velodb {

// Forward declarations
class View;

/**
 * CompactionOperator filters rows based on the $_mask column.
 * If $_mask column exists, only rows where $_mask is true are kept.
 * If $_mask column doesn't exist, all rows are passed through unchanged.
 */
class CompactionOperator : public UnaryOperator {
public:
    /**
     * Constructs a CompactionOperator.
     * @param catalog Reference to the catalog
     * @param output_schema Schema for the output
     * @param child The child operator to get input from
     */
    CompactionOperator(Catalog& catalog,
                      std::unique_ptr<Schema> output_schema,
                      std::unique_ptr<AbstractOperator> child);

    /**
     * Virtual destructor for proper cleanup.
     */
    ~CompactionOperator() override = default;

    /**
     * Executes the compaction operation.
     * @return Result containing the compacted View or error
     */
    Result<View> execute() const override;
};

} // namespace velodb
