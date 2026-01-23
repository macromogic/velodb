#pragma once

#include "cuda/commands.hpp"
#include "cuda/helper.hpp"

#include <cuda_runtime.h>

namespace velodb::cuda {

template <typename KeyT>
__device__ __forceinline__ KeyT getKeyAt(const JoinColumn& col, size_t index)
{
    const KeyT* keys = static_cast<const KeyT*>(col.keys);
    if (index >= col.n) {
        // Out-of-bounds access; return max value
        return ::cuda::std::numeric_limits<KeyT>::max();
    }
    return keys[index];
}

template <typename KeyT>
__device__ __forceinline__ size_t lowerBound(const JoinColumn& col, KeyT target)
{
    size_t left = 0;
    size_t right = col.n;
    while (left < right) {
        size_t mid = left + (right - left) / 2;
        KeyT mid_key = getKeyAt<KeyT>(col, mid);
        if (mid_key < target) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    return left;
}

template <typename KeyT>
__device__ __forceinline__ size_t upperBound(const JoinColumn& col, KeyT target)
{
    size_t left = 0;
    size_t right = col.n;
    while (left < right) {
        size_t mid = left + (right - left) / 2;
        KeyT mid_key = getKeyAt<KeyT>(col, mid);
        if (mid_key <= target) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    return left;
}

template <typename KeyT>
__device__ __forceinline__ void mergeBlockCount(const JoinColumn& left,
                                                size_t left_start,
                                                size_t left_end,
                                                const JoinColumn& right,
                                                size_t right_start,
                                                size_t right_end,
                                                MergeJoinBlock* out_blocks,
                                                size_t* block_count,
                                                size_t* n_rows)
{
    size_t l = left_start;
    size_t r = right_start;

    while (l < left_end && r < right_end) {
        KeyT left_key = getKeyAt<KeyT>(left, l);
        KeyT right_key = getKeyAt<KeyT>(right, r);

        if (left_key < right_key) {
            l++;
        } else if (left_key > right_key) {
            r++;
        } else {
            // Match found
            size_t left_match_start = l;
            size_t right_match_start = r;
            while (l < left_end && getKeyAt<KeyT>(left, l) == left_key) {
                l++;
            }
            while (r < right_end && getKeyAt<KeyT>(right, r) == right_key) {
                r++;
            }
            size_t match_count = (l - left_match_start) * (r - right_match_start);
            size_t block_idx = atomicAdd((unsigned long long*)block_count, 1);
            atomicAdd((unsigned long long*)n_rows, match_count);
            out_blocks[block_idx]
                = { left_match_start, l - left_match_start, right_match_start, r - right_match_start, match_count };
        }
    }
}

template <typename KeyT>
__device__ __forceinline__ void sortMergeJoinCountImpl(const JoinColumn& left,
                                                       const JoinColumn& right,
                                                       MergeJoinBlock* out_blocks,
                                                       size_t* block_count,
                                                       size_t* n_rows,
                                                       cg::grid_group& grid)
{
    size_t grid_block_rank = grid.block_rank();
    size_t tuples_per_block = DIV_UP(left.n, grid.num_blocks());
    size_t left_start = grid_block_rank * tuples_per_block;
    size_t left_end = min(left_start + tuples_per_block, left.n);
    if (left_start >= left_end) {
        return;
    }

    KeyT left_min_key = getKeyAt<KeyT>(left, left_start);
    KeyT left_max_key = getKeyAt<KeyT>(left, left_end);

    size_t right_start = lowerBound(right, left_min_key);
    size_t right_end = upperBound(right, left_max_key);

    if (threadIdx.x == 0) {
        mergeBlockCount<KeyT>(left,
                              left_start,
                              left_end,
                              right,
                              right_start,
                              right_end,
                              out_blocks,
                              block_count,
                              n_rows);
    }
}

__device__ __forceinline__ void writeJoinResults(const JoinColumn& left,
                                                 const JoinColumn& right,
                                                 const MergeJoinBlock& block,
                                                 int64_t* out_left,
                                                 int64_t* out_right)
{
    size_t tid = threadIdx.x;
    size_t write_block_size = block.left_size * block.right_size;
    for (size_t idx = tid; idx < write_block_size; idx += blockDim.x) {
        size_t left_idx = block.left_start + (idx / block.right_size);
        size_t right_idx = block.right_start + (idx % block.right_size);
        size_t out_idx = block.output_start + idx;
        out_left[out_idx] = left.rowids[left_idx];
        out_right[out_idx] = right.rowids[right_idx];
    }
}

__device__ __forceinline__ void sortMergeJoinWriteImpl(const JoinColumn& left,
                                                       const JoinColumn& right,
                                                       MergeJoinBlock* join_blocks,
                                                       size_t* n_join_blocks,
                                                       int64_t* out_left,
                                                       int64_t* out_right,
                                                       cg::grid_group& grid)
{
    size_t grid_block_rank = grid.block_rank();
    size_t num_grid_blocks = grid.num_blocks();
    size_t total_join_blocks = *n_join_blocks;
    for (size_t block_idx = grid_block_rank; block_idx < total_join_blocks; block_idx += num_grid_blocks) {
        writeJoinResults(left, right, join_blocks[block_idx], out_left, out_right);
    }
}

// Step 1: count matches per block
__device__ void executeSortMergeJoinCount(const CommandArgs::SortMergeJoinCountArgs& args, cg::grid_group& grid)
{
    switch (args.type_id) {
#define X(name, DT, VT)                                                                                                \
    case DataTypeId::name: {                                                                                           \
        sortMergeJoinCountImpl<DT>(args.left,                                                                          \
                                   args.right,                                                                         \
                                   args.out_blocks,                                                                    \
                                   args.out_block_count,                                                               \
                                   args.out_row_count,                                                                 \
                                   grid);                                                                              \
        break;                                                                                                         \
    }
        LIST_TYPES(X)
#undef X
    default:
        break;
    }
}

// Step 2: write matching rowids using precomputed blocks
__device__ void executeSortMergeJoinWrite(const CommandArgs::SortMergeJoinWriteArgs& args, cg::grid_group& grid)
{
    sortMergeJoinWriteImpl(args.left, args.right, args.blocks, args.n_blocks, args.out_left, args.out_right, grid);
}

} // namespace velodb::cuda
