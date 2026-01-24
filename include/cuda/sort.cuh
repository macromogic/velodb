#pragma once

#include "cuda/commands.hpp"
#include "cuda/helper.hpp"
#include "data/type_traits.hpp"

#include <cuda_runtime.h>

namespace velodb::cuda {

__device__ __forceinline__ int compareSingle(const void* ptr, DataTypeId type_id, size_t row_i, size_t row_j)
{
    switch (type_id) {
#define X(name, DT, VT)                                                                                                \
    case DataTypeId::name: {                                                                                           \
        const DT* data = static_cast<const DT*>(ptr);                                                                  \
        const DT di = data[row_i];                                                                                     \
        const DT dj = data[row_j];                                                                                     \
        if (di == dj) {                                                                                                \
            return 0;                                                                                                  \
        }                                                                                                              \
        return (di < dj) ? -1 : 1;                                                                                     \
    }
        LIST_TYPES(X)
#undef X
    default:
        return 0; // Should never reach
    }
}

__device__ __forceinline__ bool needSwap(const CommandArgs::SortArgs& args, size_t i, size_t j, bool ascending)
{
    for (int col_idx = 0; col_idx < args.n_sort_columns; col_idx++) {
        int cmp = compareSingle(args.sort_cols[col_idx], args.col_types[col_idx], i, j);
        if (cmp != 0) {
            return (ascending ^ args.ascending_flags[col_idx]) ? (cmp < 0) : (cmp > 0);
        }
    }
    return false;
}

template <typename T>
__device__ __forceinline__ void swapValues(T& a, T& b)
{
    T temp = a;
    a = b;
    b = temp;
}

__device__ __forceinline__ void swapRows(const CommandArgs::SortArgs& args, size_t i, size_t j)
{
    swapValues(args.indices[i], args.indices[j]);
    for (int col_idx = 0; col_idx < args.n_sort_columns; col_idx++) {
        void* ptr = args.sort_cols[col_idx];
        switch (args.col_types[col_idx]) {
#define X(name, DT, VT)                                                                                                \
    case DataTypeId::name: {                                                                                           \
        DT* col = static_cast<DT*>(ptr);                                                                               \
        swapValues(col[i], col[j]);                                                                                    \
        break;                                                                                                         \
    }
            LIST_TYPES(X)
#undef X
        default:
            break; // Should not reach
        }
    }
}

__device__ __forceinline__ void executeBitonicSort(CommandArgs::SortArgs& args, cg::grid_group& grid)
{
    size_t n_rows = args.n_rows;
    size_t n_padded_rows = args.n_padded_rows;

    size_t tid = grid.thread_rank();
    size_t n_threads = grid.size();

    // Initialize indices
    for (size_t i = tid; i < n_padded_rows; i += n_threads) {
        args.indices[i] = i;
        if (i >= n_rows) {
            for (int col_idx = 0; col_idx < args.n_sort_columns; col_idx++) {
                void* ptr = args.sort_cols[col_idx];
                switch (args.col_types[col_idx]) {
#define X(name, DT, VT)                                                                                                \
    case DataTypeId::name: {                                                                                           \
        DT* col = static_cast<DT*>(ptr);                                                                               \
        col[i] = args.ascending_flags[col_idx] ? ::cuda::std::numeric_limits<DT>::max()                                \
                                               : ::cuda::std::numeric_limits<DT>::lowest();                            \
        break;                                                                                                         \
    }
                    LIST_TYPES(X)
#undef X
                default:
                    break; // Should not reach
                }
            }
        }
    }
    grid.sync();

    // Bitonic sort loop
    for (size_t k = 2; k <= n_padded_rows; k <<= 1) {
        for (size_t j = k >> 1; j > 0; j >>= 1) {
            for (size_t i = tid; i < n_padded_rows; i += n_threads) {
                size_t ixj = i ^ j;
                if (i < ixj) {
                    bool ascending = ((i & k) == 0);
                    if (needSwap(args, i, ixj, ascending)) {
                        swapRows(args, i, ixj);
                    }
                }
            }
            grid.sync();
        }
    }
}

} // namespace velodb::cuda
