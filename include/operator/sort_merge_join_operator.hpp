#pragma once

#include "operator/abstract_operator.hpp"
#include "operator/join_type.hpp"

#include <memory>
#include <random>

namespace velodb {

// Forward declarations
class AbstractExpression;

class SortMergeJoinOperator : public BinaryOperator {
public:
    SortMergeJoinOperator(ExecutionContext& context,
                          Schema output_schema,
                          std::unique_ptr<AbstractOperator> left_child,
                          std::unique_ptr<AbstractOperator> right_child,
                          std::pair<size_t, size_t> join_key_indices,
                          std::vector<const Table*> left_source_tables,
                          std::vector<const Table*> right_source_tables,
                          JoinType join_type = JoinType::INNER);
    ~SortMergeJoinOperator() override = default;

    Result<RowBatch> next() override;

private:
    std::pair<size_t, size_t> join_key_indices_;
    std::vector<const Table*> left_source_tables_;
    std::vector<const Table*> right_source_tables_;
    JoinType join_type_;
    bool joined_ { false };
};

} // namespace velodb
