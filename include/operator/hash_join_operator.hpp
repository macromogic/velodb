#pragma once

#include "operator/abstract_operator.hpp"
#include "operator/join_type.hpp"

#include <memory>
#include <random>

#include <cuda_runtime.h>

namespace velodb {

// Forward declarations
class AbstractExpression;

class HashJoinOperator : public BinaryOperator {
public:
    HashJoinOperator(ExecutionContext& context,
                     Schema output_schema,
                     std::unique_ptr<AbstractOperator> left_child,
                     std::unique_ptr<AbstractOperator> right_child,
                     std::pair<size_t, size_t> join_key_indices,
                     std::vector<const Table*> left_source_tables,
                     std::vector<const Table*> right_source_tables,
                     JoinType join_type = JoinType::INNER);
    ~HashJoinOperator() override;

    Result<RowBatch> next() override;

private:
    int64_t getSeed()
    {
        uint64_t seed = (static_cast<uint64_t>(rd_()) << 32) | rd_();
        return static_cast<int64_t>(seed);
    }

    void initHashTable(DataTypeId type_id, uint32_t capacity, uint32_t num_buckets);
    bool buildHashTable();

    Result<RowBatch> probeWithBatch(RowBatch& probe_batch);

    void cleanup();

    // Configuration
    std::pair<size_t, size_t> join_key_indices_;
    std::vector<const Table*> left_source_tables_;
    std::vector<const Table*> right_source_tables_;
    JoinType join_type_;
    std::random_device rd_ {};

    // State: simple flags instead of state machine
    bool hash_table_built_ { false };
    bool finished_ { false };

    // Build side data (kept for the duration of probing)
    RowBatch build_batch_;
    size_t build_size_ { 0 };
    DataTypeId key_type_id_;

    HashTable ht_ {};
};

} // namespace velodb
