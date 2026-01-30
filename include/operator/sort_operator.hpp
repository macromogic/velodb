#pragma once

#include "operator/abstract_operator.hpp"

#include <memory>

namespace velodb {

// Forward declarations
class AbstractExpression;

// Sort operator - supports incremental batch output
class SortOperator : public UnaryOperator {
public:
    SortOperator(ExecutionContext& context,
                 Schema output_schema,
                 std::unique_ptr<AbstractOperator> child,
                 std::vector<size_t> order_indices,
                 std::vector<bool> ascending_flags);
    ~SortOperator() override = default;

    Result<RowBatch> next() override;

private:
    std::vector<size_t> order_indices_;
    std::vector<bool> ascending_flags_;
    bool sorted_ { false };

    // For incremental batch output
    RowBatch sorted_batch_; // Complete sorted result (kept on CUDA)
    size_t current_offset_ { 0 }; // Current offset for incremental output
};

} // namespace velodb
