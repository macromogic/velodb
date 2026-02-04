#pragma once

#include "expression/expression.hpp"
#include "operator/abstract_operator.hpp"

#include <memory>

namespace velodb {

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
