#pragma once

#include "operator/abstract_operator.hpp"
#include "operator/join_type.hpp"

#include <memory>
#include <random>

#include <cuda_runtime.h>

namespace velodb {

// Forward declarations
class AbstractExpression;

/**
 * @brief Streaming Hash Join Operator
 *
 * Build phase: Collects all rows from build side (left child) and constructs hash table.
 * Probe phase: Streams probe batches from right child, probing hash table per batch.
 *
 * This streaming approach avoids creating a single massive output batch, which:
 * 1. Reduces peak memory usage
 * 2. Allows downstream operators (like MaterializationOperator) to process smaller
 *    batches with much less bitonic sort padding overhead
 */
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

    /**
     * @brief Build the hash table from left child (one-time operation)
     * @return true if build succeeded and has data, false if empty
     */
    bool buildHashTable();

    /**
     * @brief Probe hash table with a single batch from probe side
     */
    Result<RowBatch> probeWithBatch(RowBatch& probe_batch);

    /**
     * @brief Cleanup GPU resources
     */
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
    DataTypeId key_type_id_ { DataTypeId::INTEGER };

    // GPU Hash Table (persists across probe calls)
    void* d_ht_entries_ { nullptr };
    uint32_t* d_ht_heads_ { nullptr };
    uint32_t ht_capacity_ { 0 };
    uint32_t ht_num_buckets_ { 0 };
};

} // namespace velodb
