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
                          const Table& left_table,
                          const Table& right_table,
                          JoinType join_type = JoinType::INNER);
    ~SortMergeJoinOperator() override = default;

    Result<RowBatch> next() override;

private:
    int64_t getSeed()
    {
        uint64_t seed = (static_cast<uint64_t>(rd_()) << 32) | rd_();
        return static_cast<int64_t>(seed);
    }

    std::reference_wrapper<const Table> left_table_;
    std::reference_wrapper<const Table> right_table_;
    JoinType join_type_;
    bool joined_ { false };
    std::random_device rd_ {};
};

} // namespace velodb
