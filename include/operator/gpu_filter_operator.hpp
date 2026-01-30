#pragma once

#include "expression/expression.hpp"
#include "operator/abstract_operator.hpp"

#include <memory>

namespace velodb {

/**
 * @brief GPU-based filter operator.
 *
 * Evaluates a predicate on GPU data and compacts the result.
 * This is used for post-join filtering when some join predicates
 * couldn't be applied during the join itself.
 */
class GpuFilterOperator : public UnaryOperator {
public:
    GpuFilterOperator(ExecutionContext& context,
                      Schema output_schema,
                      std::unique_ptr<AbstractOperator> child,
                      std::unique_ptr<AbstractExpression> predicate);
    ~GpuFilterOperator() override = default;

    Result<RowBatch> next() override;

private:
    std::unique_ptr<AbstractExpression> predicate_;
};

} // namespace velodb
