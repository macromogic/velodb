#pragma once

#include "data/bit_vector.hpp"
#include "data/type_traits.hpp"

#include <cstddef>
#include <cstdint>

#include <cuda_runtime.h>

namespace velodb {

template <typename KeyT>
void materializeArray(KeyT* dst,
                      BitVector::Element* dst_bitmap,
                      const KeyT* src,
                      BitVector::Element* src_bitmap,
                      const int64_t* rowids,
                      size_t row_count,
                      cudaStream_t stream = 0);

#define X(name, DT, VT)                                                                                                \
    extern template void materializeArray<DT>(DT * dst,                                                                \
                                              BitVector::Element*,                                                     \
                                              const DT*,                                                               \
                                              BitVector::Element*,                                                     \
                                              const int64_t*,                                                          \
                                              size_t,                                                                  \
                                              cudaStream_t);
LIST_TYPES(X)
#undef X

} // namespace velodb
