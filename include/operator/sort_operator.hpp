#pragma once

#include "operator/abstract_operator.hpp"

#include <memory>
#include <vector>

namespace velodb {

// Forward declarations
class AbstractExpression;

// Sort operator
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
    RowBatch buffer_;
    bool sorted_ { false };
};

} // namespace velodb
