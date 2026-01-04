#include "cuda/helper.hpp"
#include "cuda/sort.hpp"
#include "data/type_traits.hpp"

#include <cstdint>

#include <cuda_runtime.h>

namespace velodb {

namespace gpu {

    __device__ __forceinline__ int compareSingle(int64_t row_i, int64_t row_j, const SortColumn& col, bool reverse)
    {
        switch (col.id) {
#define X(name, DT, VT)                                                                                                \
    case DataTypeId::name: {                                                                                           \
        const DT* data = static_cast<const DT*>(col.data);                                                             \
        bool ascending = reverse ^ col.ascending;                                                                      \
        if (data[row_i] < data[row_j])                                                                                 \
            return ascending ? -1 : 1;                                                                                 \
        if (data[row_i] > data[row_j])                                                                                 \
            return ascending ? 1 : -1;                                                                                 \
        return 0;                                                                                                      \
    }
            LIST_TYPES(X)
#undef X
        default:
            return 0; // TODO: should never reach
        }
    }

    __device__ bool multiKeyLess(int64_t row_i, int64_t row_j, SortColumn* cols, size_t n_cols, bool reverse)
    {
        if (row_i == -1)
            return false; // row -1 is always the largest
        if (row_j == -1)
            return true;

        for (size_t k = 0; k < n_cols; k++) {
            int cmp = compareSingle(row_i, row_j, cols[k], reverse);
            if (cmp < 0)
                return true;
            if (cmp > 0)
                return false;
        }
        return row_i < row_j;
    }

    __global__ void initializeIndicesKernel(int64_t* d_indices, size_t n)
    {
        unsigned int i = threadIdx.x + blockIdx.x * blockDim.x;
        if (i < n) {
            d_indices[i] = static_cast<int64_t>(i);
        }
    }

    __global__ void bitonicStepKernel(int64_t* d_row_ids,
                                      SortColumn* d_sort_columns,
                                      size_t n_sort_columns,
                                      unsigned int j,
                                      unsigned int k,
                                      unsigned int N,
                                      bool reverse)
    {
        unsigned int i = threadIdx.x + blockIdx.x * blockDim.x;
        unsigned int ixj = i ^ j;

        if (ixj > i && ixj < N && i < N) {
            bool ascending = ((i & k) == 0);
            bool should_swap = multiKeyLess(d_row_ids[ixj], d_row_ids[i], d_sort_columns, n_sort_columns, reverse);
            if (ascending ? should_swap : !should_swap) {
                int64_t tmp = d_row_ids[i];
                d_row_ids[i] = d_row_ids[ixj];
                d_row_ids[ixj] = tmp;
            }
        }
    }

    template <typename T>
    __global__ void reorderKernel(T* d_out, const T* d_in, const int64_t* d_indices, size_t n)
    {
        unsigned int i = threadIdx.x + blockIdx.x * blockDim.x;
        if (i < n) {
            int64_t idx = d_indices[i];
            d_out[i] = d_in[idx];
        }
    }

    template <typename Elem>
    __global__ void reorderBitmapKernel(Elem* d_out, const Elem* d_in, const int64_t* d_indices, size_t n)
    {
        constexpr auto ElemSize = sizeof(Elem) * 8;

        unsigned int i = threadIdx.x + blockIdx.x * blockDim.x;
        unsigned int warp_id = i >> WARP_BITS;
        unsigned int lane_id = i & WARP_MASK;
        unsigned int mask = __activemask();

        int bit = 0;
        if (i < n) {
            int64_t idx = d_indices[i];
            bit = d_in[idx / ElemSize] >> (idx % ElemSize) & 1;
        }

        uint32_t ballot = __ballot_sync(mask, bit);
        if (lane_id == 0) {
            reinterpret_cast<uint32_t*>(d_out)[warp_id] = ballot;
        }
    }

} // namespace gpu

int64_t* initializeIndices(size_t n, cudaStream_t stream)
{
    if (n == 0)
        return nullptr;

    int64_t* d_indices;
    CHECKED_CALL_THROW(cudaMalloc(&d_indices, n * sizeof(int64_t)));

    int threads = 256;
    int blocks = DIV_UP(n, threads);
    gpu::initializeIndicesKernel<<<blocks, threads, 0, stream>>>(d_indices, n);
    return d_indices;
}

void sortIndices(int64_t* d_row_ids,
                 size_t n_rows,
                 SortColumn* d_sort_columns,
                 size_t n_sort_columns,
                 cudaStream_t stream,
                 size_t min_block_size,
                 bool reverse)
{
    if (n_rows <= 1)
        return;

    size_t padded_rows = 1ull << static_cast<int>(ceil(log2((double)n_rows)));

    int64_t* d_padded_row_ids;
    CHECKED_CALL_THROW(cudaMalloc(&d_padded_row_ids, padded_rows * sizeof(int64_t)));
    CHECKED_CALL_THROW(cudaMemset(d_padded_row_ids, -1, padded_rows * sizeof(int64_t)));
    CHECKED_CALL_THROW(
        cudaMemcpyAsync(d_padded_row_ids, d_row_ids, n_rows * sizeof(int64_t), cudaMemcpyDeviceToDevice, stream));

    int threads = 256;
    int blocks = DIV_UP(padded_rows, threads);
    for (size_t k = min_block_size * 2; k <= padded_rows; k <<= 1) {
        for (size_t j = k >> 1; j >= min_block_size; j >>= 1) {
            gpu::bitonicStepKernel<<<blocks, threads, 0, stream>>>(d_padded_row_ids,
                                                                   d_sort_columns,
                                                                   n_sort_columns,
                                                                   j,
                                                                   k,
                                                                   padded_rows,
                                                                   reverse);
        }
    }

    CHECKED_CALL_THROW(
        cudaMemcpyAsync(d_row_ids, d_padded_row_ids, n_rows * sizeof(int64_t), cudaMemcpyDeviceToDevice, stream));
    CHECKED_CALL_THROW(cudaFree(d_padded_row_ids));
}

template <typename T>
T* reorderData(T* d_data, const int64_t* d_indices, size_t n, cudaStream_t stream)
{
    if (n == 0)
        return nullptr;

    T* d_out;
    CHECKED_CALL_THROW(cudaMalloc(&d_out, n * sizeof(T)));

    // Launch kernel to reorder data
    int threads = 256;
    int blocks = DIV_UP(n, threads);
    gpu::reorderKernel<<<blocks, threads, 0, stream>>>(d_out, d_data, d_indices, n);
    return d_out;
}

template <typename Elem>
Elem* reorderBitmap(Elem* d_bitmap, const int64_t* d_indices, size_t n, cudaStream_t stream)
{
    if (n == 0)
        return nullptr;

    Elem* d_out;
    CHECKED_CALL_THROW(cudaMalloc(&d_out, n * sizeof(Elem)));
    CHECKED_CALL_THROW(cudaMemsetAsync(d_out, 0, n * sizeof(Elem), stream));

    // Launch kernel to reorder data
    int threads = 256;
    int blocks = DIV_UP(n, threads);
    gpu::reorderBitmapKernel<Elem><<<blocks, threads, 0, stream>>>(d_out, d_bitmap, d_indices, n);
    return d_out;
}

// Explicit template instantiations
#define X(name, DT, VT) template DT* reorderData<DT>(DT*, const int64_t*, size_t, cudaStream_t);
LIST_TYPES(X)
#undef X

template BitVector::Element* reorderBitmap<BitVector::Element>(BitVector::Element*,
                                                               const int64_t*,
                                                               size_t,
                                                               cudaStream_t);

} // namespace velodb
