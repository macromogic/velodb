#include "cuda/helper.hpp"
#include "cuda/materialization.hpp"
#include "data/type_traits.hpp"

#include <cstddef>
#include <cstdint>

#include <cuda.h>
#include <cuda_runtime.h>

namespace velodb {

namespace gpu {

    template <typename KeyT>
    __global__ void materializeArrayKernel(KeyT* dest, const KeyT* source, const int64_t* rowids, size_t row_count)
    {
        size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
        size_t stride = blockDim.x * gridDim.x;
        for (size_t i = idx; i < row_count; i += stride) {
            dest[i] = source[rowids[i]];
        }
    }

    __global__ void materializeBitmapKernel(BitVector::Element* dest_bitmap,
                                            const BitVector::Element* source_bitmap,
                                            const int64_t* rowids,
                                            size_t row_count)
    {
        size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
        size_t stride = blockDim.x * gridDim.x;
        for (size_t i = idx; i < row_count; i += stride) {
            size_t source_bit_index = rowids[i];
            bool bit_value = (source_bitmap[source_bit_index / BitVector::ELEMENT_WIDTH]
                              >> (source_bit_index % BitVector::ELEMENT_WIDTH))
                & 1;
            if (bit_value) {
                dest_bitmap[i / BitVector::ELEMENT_WIDTH] |= (BitVector::Element(1) << (i % BitVector::ELEMENT_WIDTH));
            } else {
                dest_bitmap[i / BitVector::ELEMENT_WIDTH] &= ~(BitVector::Element(1) << (i % BitVector::ELEMENT_WIDTH));
            }
        }
    }

} // namespace gpu

template <typename KeyT>
void materializeArray(KeyT* dst,
                      BitVector::Element* dst_bitmap,
                      const KeyT* src,
                      BitVector::Element* src_bitmap,
                      const int64_t* rowids,
                      size_t row_count,
                      cudaStream_t stream)
{
    size_t block_size = 256;
    size_t grid_size = (row_count + block_size - 1) / block_size;
    gpu::materializeArrayKernel<KeyT><<<grid_size, block_size, 0, stream>>>(dst, src, rowids, row_count);
    CHECKED_CALL_THROW(cudaGetLastError());
    gpu::materializeBitmapKernel<<<grid_size, block_size, 0, stream>>>(dst_bitmap, src_bitmap, rowids, row_count);
    CHECKED_CALL_THROW(cudaGetLastError());
}

#define X(name, DT, VT)                                                                                                \
    template void materializeArray<DT>(DT * dst,                                                                       \
                                       BitVector::Element*,                                                            \
                                       const DT*,                                                                      \
                                       BitVector::Element*,                                                            \
                                       const int64_t*,                                                                 \
                                       size_t,                                                                         \
                                       cudaStream_t);
LIST_TYPES(X)
#undef X

} // namespace velodb
