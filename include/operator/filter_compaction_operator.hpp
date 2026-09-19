#pragma once

#include "operator/abstract_operator.hpp"

#include <future>
#include <memory>
#include <optional>
#include <vector>

namespace velodb {

class FilterCompactionOperator : public UnaryOperator {
public:
    FilterCompactionOperator(ExecutionContext& context,
                             Schema output_schema,
                             std::unique_ptr<AbstractOperator> child,
                             std::vector<bool> compact_columns = {});
    ~FilterCompactionOperator();
    Result<RowBatch> next() override;

private:
    // Prefetch state for double-buffering
    std::optional<RowBatch> prefetched_batch_;
    std::future<void> prefetch_future_;
    bool first_call_ = true;
    std::vector<bool> compact_columns_;

    // Start async prefetch of next batch
    void startPrefetch();

    // Wait for prefetch to complete and get the batch
    std::optional<RowBatch> waitPrefetch();

    // Process a batch that's already on GPU
    Result<RowBatch> processBatchOnGpu(RowBatch& batch);
};

} // namespace velodb
