#pragma once

#include "operator/abstract_operator.hpp"

#include <memory>

namespace velodb {

// Limit operator
class LimitOperator : public UnaryOperator {
public:
    LimitOperator(ExecutionContext& context,
                  Schema output_schema,
                  std::unique_ptr<AbstractOperator> child,
                  size_t limit,
                  size_t offset = 0);
    ~LimitOperator() override = default;

    Result<RowBatch> next() override;

private:
    size_t limit_;
    size_t offset_;
    size_t current_count_;
    size_t skipped_count_;
};

} // namespace velodb
