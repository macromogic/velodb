#pragma once

#include "operator/abstract_operator.hpp"
#include "operator/join_type.hpp"

#include <memory>

namespace velodb {

// Forward declarations
class AbstractExpression;

class SortMergeJoinOperator : public BinaryOperator {
public:
    SortMergeJoinOperator(ExecutionContext& context,
                          Schema output_schema,
                          std::unique_ptr<AbstractOperator> left_child,
                          std::unique_ptr<AbstractOperator> right_child,
                          std::unique_ptr<AbstractExpression> left_key_expr,
                          std::unique_ptr<AbstractExpression> right_key_expr,
                          JoinType join_type = JoinType::INNER);
    ~SortMergeJoinOperator() override = default;

    Result<RowBatch> next() override;

private:
    std::unique_ptr<AbstractExpression> left_key_expr_;
    std::unique_ptr<AbstractExpression> right_key_expr_;
    JoinType join_type_;
    bool joined_ { false };
};

} // namespace velodb
