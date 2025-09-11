#pragma once

#include "data/type_traits.hpp"

#include <cstddef>
#include <cstdint>

#include <cuda_runtime.h>

namespace velodb {

template <typename T>
size_t filter_compact(T* dst, const T* src, const uint8_t* mask, size_t n, cudaStream_t stream = 0, int block = 256);

#define X(name, DT, VT)                                                                                                \
    extern template size_t filter_compact<DT>(DT*, const DT*, const uint8_t*, size_t, cudaStream_t, int);
LIST_TYPES(X)
#undef X

} // namespace velodb
