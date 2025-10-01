#pragma once

#include "data/data_type.hpp"
#include "data/type_traits.hpp"

#include <cstddef>
#include <cstdint>

#include <cuda_runtime.h>

namespace velodb {

struct SortColumn {
    const void* data;
    DataTypeId id;
    bool ascending;
};

void sortIndices(int64_t* d_row_ids,
                 size_t n_rows,
                 SortColumn* d_sort_columns,
                 size_t n_sort_columns,
                 cudaStream_t stream = 0,
                 size_t min_block_size = 1,
                 bool reverse = false);

template <typename T>
T* reorderData(T* d_data, const int64_t* d_indices, size_t n, cudaStream_t stream = 0);

#define X(name, DT, VT) extern template DT* reorderData<DT>(DT*, const int64_t*, size_t, cudaStream_t);
LIST_TYPES(X)
#undef X

} // namespace velodb
