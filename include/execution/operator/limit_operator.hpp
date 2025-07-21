#pragma once

#include "execution/operator/abstract_operator.hpp"
#include <memory>

namespace velodb {

// Limit operator
class LimitOperator : public AbstractOperator {
public:
    LimitOperator(std::unique_ptr<AbstractOperator> child, size_t limit, size_t offset = 0);
    ~LimitOperator() override = default;

    void init() override;
    void reset() override;

    // Late materialization interface
    bool nextRowId(RowId* row_id) override;

private:
    std::unique_ptr<AbstractOperator> child_;
    size_t limit_;
    size_t offset_;
    size_t current_count_ { 0 };
    size_t skipped_count_ { 0 };
};

} // namespace velodb
