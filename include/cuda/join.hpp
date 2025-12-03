#pragma once

#include "data/type_traits.hpp"

#include <cstddef>
#include <cstdint>

#include <cuda_runtime.h>

namespace velodb {

template <typename KeyT>
struct JoinColumn {
    const KeyT* keys;
    const int64_t* rowids;
    size_t row_count;
};

struct JoinResult {
    int64_t* out_left;
    int64_t* out_right;
    size_t row_count;
};

// Performs a sort-merge inner equijoin between two sorted key arrays on device.
// left_keys, right_keys: device pointers to sorted key arrays
// left_rowids, right_rowids: device pointers to row-id payloads (can be nullptr -> use implicit 0..n-1)
// out_left/out_right: device pointers preallocated for output pairs
// max_out: maximum number of pairs that can be written to out_* buffers
// out_count: host pointer to receive the number of pairs produced (written by the function)
// stream: optional CUDA stream
// Note: arrays must be located on device and sorted ascending. Only inner equijoin on a single key
// column is supported. This function reserves temporary device memory internally.
template <typename KeyT>
JoinResult sortMergeJoin(const JoinColumn<KeyT>& left, const JoinColumn<KeyT>& right, cudaStream_t stream = 0);

// Explicit instantiations for supported device key types. Rowid / output types are int64_t.
#define X(name, DT, VT)                                                                                                \
    extern template JoinResult sortMergeJoin<DT>(const JoinColumn<DT>&, const JoinColumn<DT>&, cudaStream_t);
LIST_TYPES(X)
#undef X

} // namespace velodb
