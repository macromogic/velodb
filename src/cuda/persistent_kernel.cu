#include "data/data_type.hpp"
#include "data/type_traits.hpp"

#include "cuda/compare.cuh"
#include "cuda/gather.cuh"
#include "cuda/hash_join.cuh"
#include "cuda/permute.cuh"
#include "cuda/persistent_kernel.cuh"
#include "cuda/scatter.cuh"
#include "cuda/sort.cuh"
#include "cuda/sort_merge_join.cuh"

#include <cuda_runtime.h>

namespace velodb::cuda {

__device__ __forceinline__ void dispatchCommand(Command& cmd, cg::grid_group& grid)
{
    switch (cmd.opcode) {
    case OpCode::OP_SCATTER:
        executeScatter(cmd.args.scatter, grid);
        break;

    case OpCode::OP_GATHER:
        executeGather(cmd.args.gather, grid);
        break;

    case OpCode::OP_PERMUTE:
        executePermute(cmd.args.permute, grid);
        break;

    case OpCode::OP_SORT:
        executeBitonicSort(cmd.args.sort, grid);
        break;

    case OpCode::OP_SORT_MERGE_JOIN_COUNT:
        executeSortMergeJoinCount(cmd.args.sort_merge_join_count, grid);
        break;

    case OpCode::OP_SORT_MERGE_JOIN_PREPARE:
        executeSortMergeJoinPrepare(cmd.args.sort_merge_join_prepare, grid);
        break;

    case OpCode::OP_SORT_MERGE_JOIN_WRITE:
        executeSortMergeJoinWrite(cmd.args.sort_merge_join_write, grid);
        break;

    case OpCode::OP_HASH_JOIN_BUILD:
        executeHashJoinBuild(cmd.args.hash_join_build, grid);
        break;

    case OpCode::OP_HASH_JOIN_COUNT:
        executeHashJoinCount(cmd.args.hash_join_count, grid);
        break;

    case OpCode::OP_HASH_JOIN_WRITE:
        executeHashJoinWrite(cmd.args.hash_join_write, grid);
        break;

    case OpCode::OP_COMPARE_EQ:
        executeCompareEq(cmd.args.compare_eq, grid);
        break;

    case OpCode::OP_NOP: // Do nothing
    case OpCode::OP_TERMINATE: // Will be handled in main loop
    default: // Invalid opcode - ignore
        break;
    }
}

__global__ void persistentKernel(CommandQueue queue)
{
    cg::grid_group grid = cg::this_grid();
    bool is_leader = (grid.thread_rank() == 0);
    uint32_t local_tail = 0;

    grid.sync();
    while (true) {
        Command cmd;

        // Query head from ring buffer
        while (true) {
            if (is_leader) {
                uint32_t h_head = queue.host_ctrl->head;
                if (h_head != queue.device_status->internal_head) {
                    queue.device_status->internal_head = h_head;
                    __threadfence();
                }
            }

            if (queue.device_status->internal_head != local_tail) {
                cmd = queue.ring_buffer[local_tail];
                break;
            }

            __nanosleep(100);
            // __threadfence();
        }

        bool should_terminate = (cmd.opcode == OpCode::OP_TERMINATE);
        if (!should_terminate) {
            dispatchCommand(cmd, grid);
        }
        grid.sync();

        uint32_t next_tail = (local_tail + 1) & queue.capacity_mask;
        if (is_leader) {
            queue.host_ctrl->last_finished_id = cmd.sequence_id;
            __threadfence_system();
            queue.host_ctrl->tail = next_tail;
            __threadfence_system();
        }

        if (should_terminate) {
            break;
        }
        local_tail = next_tail;
    }
}

} // namespace velodb::cuda
