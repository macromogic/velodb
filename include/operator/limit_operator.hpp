#pragma once

#include "operator/abstract_operator.hpp"
#include <memory>

namespace velodb {

// Limit operator
class LimitOperator : public UnaryOperator {
public:
    LimitOperator(Catalog& catalog,
        std::unique_ptr<Schema> output_schema,
        std::unique_ptr<AbstractOperator> child,
        size_t limit,
        size_t offset = 0);
    ~LimitOperator() override = default;

    Result<View> execute() const override;

private:
    size_t limit_;
    size_t offset_;
    size_t current_count_ { 0 };
    size_t skipped_count_ { 0 };
};

} // namespace velodb
