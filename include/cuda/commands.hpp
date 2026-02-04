#pragma once

#include "common/constants.hpp"
#include "common/copy_traits.hpp"
#include "cuda/hash_table.hpp"
#include "data/data_type.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>

#include <cuda_runtime.h>

namespace velodb {

enum class OpCode : uint32_t {
    OP_NOP = 0,
    OP_SCATTER,
    OP_GATHER,
    OP_SORT,
    OP_PERMUTE,
    OP_SORT_MERGE_JOIN_COUNT,
    OP_SORT_MERGE_JOIN_WRITE,
    OP_HASH_JOIN_BUILD,
    OP_HASH_JOIN_COUNT,
    OP_HASH_JOIN_WRITE,
    OP_COMPARE_EQ,
    OP_COMPOSE_POSITION_MAP,
    OP_ENRICH_POSITIONS,
    OP_TERMINATE = 0xFFFFFFFF
};

struct JoinColumn {
    void* keys;
    int64_t* rowids;
    size_t n;
};

struct MergeJoinBlock {
    size_t left_start;
    size_t left_size;
    size_t right_start;
    size_t right_size;
    size_t output_start;
};

union alignas(16) CommandArgs {
    struct ScatterArgs {
        int32_t* out_indices;
        size_t* out_count;
        const uint8_t* in_mask;
        size_t n;
    } scatter;

    struct GatherArgs {
        void* out_data;
        const void* in_data;
        const int32_t* in_indices;
        const uint8_t* in_mask;
        size_t n;
        DataTypeId type_id;
    } gather;

    struct SortArgs {
        void* sort_cols[MAX_SORT_COLUMNS];
        int64_t* indices;
        size_t n_sort_columns;
        size_t n_rows;
        size_t n_padded_rows;
        DataTypeId col_types[MAX_SORT_COLUMNS];
        bool ascending_flags[MAX_SORT_COLUMNS];
    } sort;

    struct PermuteArgs {
        void* out_data;
        const void* in_data;
        const int64_t* in_indices;
        size_t n;
        DataTypeId type_id;
    } permute;

    struct SortMergeJoinCountArgs {
        JoinColumn left;
        JoinColumn right;
        MergeJoinBlock* out_blocks;
        size_t* out_block_count;
        size_t* out_row_count;
        DataTypeId type_id;
    } sort_merge_join_count;

    struct SortMergeJoinWriteArgs {
        JoinColumn left;
        JoinColumn right;
        MergeJoinBlock* blocks;
        size_t* n_blocks;
        int64_t* out_left;
        int64_t* out_right;
    } sort_merge_join_write;

    struct HashJoinBuildArgs {
        void* keys;
        const uint8_t* mask;
        size_t n;
        HashTable ht;
        DataTypeId type_id;
    } hash_join_build;

    struct HashJoinCountArgs {
        void* probe_keys;
        const uint8_t* probe_mask;
        size_t probe_n;
        HashTable ht;
        size_t* out_count;
        DataTypeId type_id;
    } hash_join_count;

    struct HashJoinWriteArgs {
        void* probe_keys;
        const uint8_t* probe_mask;
        int64_t* probe_rowids;
        size_t probe_n;
        HashTable ht;
        int64_t* out_left;
        int64_t* out_right;
        uint32_t* write_offset;
        DataTypeId type_id;
    } hash_join_write;

    struct CompareEqArgs {
        const void* left_data;
        const void* right_data;
        uint8_t* out_mask;
        size_t n;
        DataTypeId type_id;
    } compare_eq;

    struct ComposePositionMapArgs {
        const uint32_t* old_map;
        const uint32_t* sigma;
        uint32_t* new_map;
        size_t n;
    } compose_position_map;

    struct EnrichPositionsArgs {
        const uint32_t* record_ids;
        const uint32_t* position_map;
        uint32_t* positions;
        size_t n;
    } enrich_positions;
};

struct alignas(16) Command {
    uint64_t sequence_id;
    OpCode opcode;
    CommandArgs args;
};

struct HostControl {
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint64_t last_finished_id;
};

struct DeviceStatus {
    volatile uint32_t internal_head;
    uint32_t block_counter;
};

struct BufCtrl {
    uint32_t head;
    uint32_t tail;
};

struct CommandQueue {
    Command* ring_buffer;
    HostControl* host_ctrl;
    DeviceStatus* device_status;
    uint32_t capacity;
    uint32_t capacity_mask;
};

class CommandQueueController : private NonCopyable {
public:
    CommandQueueController(uint32_t capacity = 1024);
    ~CommandQueueController();
    CommandQueueController(CommandQueueController&&) = default;
    CommandQueueController& operator=(CommandQueueController&&) = default;

    CommandQueue getQueue() const { return queue_; }
    HostControl* getHostCtrl() const { return queue_.host_ctrl; }
    cudaStream_t getStream() const { return stream_; }

    uint64_t push(Command& cmd);
    void wait(uint64_t wait_id);

private:
    CommandQueue queue_;
    uint32_t cached_tail_;
    uint64_t current_id_;
    cudaStream_t stream_;
};

} // namespace velodb
