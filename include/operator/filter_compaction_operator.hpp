#pragma once

#include "operator/abstract_operator.hpp"

#include <memory>

namespace velodb {

class FilterCompactionOperator : public UnaryOperator {
public:
    FilterCompactionOperator(ExecutionContext& context, Schema output_schema, std::unique_ptr<AbstractOperator> child);
    ~FilterCompactionOperator() = default;
    Result<RowBatch> next() override;

private:
    RowBatch buffer_;
};

} // namespace velodb
