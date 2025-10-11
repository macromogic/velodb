#pragma once

#include "data/bit_vector.hpp"
#include "data/type_traits.hpp"

#include <cstddef>
#include <cstdint>

#include <cuda_runtime.h>

namespace velodb {

template <typename T>
size_t filterCompact(T* dst_data,
                     BitVector::Element* dst_bitmap,
                     const T* src_data,
                     const BitVector::Element* src_bitmap,
                     const uint8_t* mask,
                     size_t n,
                     size_t bits_offset,
                     cudaStream_t stream,
                     unsigned int block = 256);

#define X(name, DT, VT)                                                                                                \
    extern template size_t filterCompact<DT>(DT*,                                                                      \
                                             BitVector::Element*,                                                      \
                                             const DT*,                                                                \
                                             const BitVector::Element*,                                                \
                                             const uint8_t*,                                                           \
                                             size_t,                                                                   \
                                             size_t,                                                                   \
                                             cudaStream_t,                                                             \
                                             unsigned int);
LIST_TYPES(X)
#undef X

} // namespace velodb
