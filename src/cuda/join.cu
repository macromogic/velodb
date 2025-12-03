#include "cuda/helper.hpp"
#include "cuda/join.hpp"
#include "data/type_traits.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <cuda.h>
#include <cuda_runtime.h>

namespace velodb {

namespace gpu {

    template <typename T>
    __device__ __host__ inline T dev_min(T a, T b)
    {
        return a < b ? a : b;
    }

    template <typename KeyT>
    __device__ size_t mergePathSplit(const KeyT* A, size_t m, const KeyT* B, size_t n, size_t k)
    {
        size_t low = (k > n) ? (k - n) : 0;
        size_t high = (k < m) ? k : m;
        while (low < high) {
            size_t mid = (low + high + 1) >> 1;
            size_t j = k - mid;
            KeyT a_val = A[mid - 1];
            KeyT b_val = B[j];
            if (a_val <= b_val)
                low = mid;
            else
                high = mid - 1;
        }
        return low;
    }

    template <typename KeyT>
    __global__ void countPairsKernel(const KeyT* left_keys,
                                     size_t left_n,
                                     const KeyT* right_keys,
                                     size_t right_n,
                                     size_t* partition_counts,
                                     size_t total_partitions)
    {
        const size_t total = left_n + right_n;
        const size_t tid = static_cast<size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
        if (tid >= total_partitions) {
            return;
        }

        size_t chunk = DIV_UP(total, total_partitions);
        size_t diag_start = tid * chunk;
        size_t diag_end = dev_min(total, diag_start + chunk);
        if (diag_start >= diag_end) {
            partition_counts[tid] = 0ULL;
            return;
        }

        size_t i = mergePathSplit(left_keys, left_n, right_keys, right_n, diag_start);
        size_t j = diag_start - i;
        unsigned long long local_count = 0ULL;

        while ((i < left_n) && (j < right_n) && (diag_start < diag_end)) {
            KeyT a = left_keys[i];
            KeyT b = right_keys[j];
            if (a < b) {
                ++i;
                ++diag_start;
            } else if (a > b) {
                ++j;
                ++diag_start;
            } else {
                size_t i0 = i, j0 = j;
                KeyT val = a;
                while (i < left_n && left_keys[i] == val) {
                    ++i;
                }
                while (j < right_n && right_keys[j] == val) {
                    ++j;
                }
                size_t ilen = i - i0;
                size_t jlen = j - j0;
                size_t available = diag_end - diag_start;
                if (ilen + jlen <= available) {
                    local_count += static_cast<unsigned long long>(ilen) * static_cast<unsigned long long>(jlen);
                    diag_start += ilen + jlen;
                } else {
                    size_t takeL = ilen < available ? ilen : available;
                    size_t takeR = jlen < available ? jlen : available;
                    local_count += static_cast<unsigned long long>(takeL) * static_cast<unsigned long long>(takeR);
                    diag_start = diag_end;
                }
            }
        }
        partition_counts[tid] = local_count;
    }

    template <typename KeyT>
    __global__ void writePairsKernel(const KeyT* left_keys,
                                     size_t left_n,
                                     const int64_t* left_rowids,
                                     const KeyT* right_keys,
                                     size_t right_n,
                                     const int64_t* right_rowids,
                                     int64_t* out_left,
                                     int64_t* out_right,
                                     const size_t* partition_offsets,
                                     size_t total_partitions)
    {
        const size_t total = left_n + right_n;
        const size_t tid = static_cast<size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
        if (tid >= total_partitions) {
            return;
        }

        size_t chunk = DIV_UP(total, total_partitions);
        size_t diag_start = tid * chunk;
        size_t diag_end = dev_min(total, diag_start + chunk);
        if (diag_start >= diag_end) {
            return;
        }

        size_t i = mergePathSplit(left_keys, left_n, right_keys, right_n, diag_start);
        size_t j = diag_start - i;
        unsigned long long write_pos = partition_offsets[tid];

        while ((i < left_n) && (j < right_n) && (diag_start < diag_end)) {
            KeyT a = left_keys[i];
            KeyT b = right_keys[j];
            if (a < b) {
                ++i;
                ++diag_start;
            } else if (a > b) {
                ++j;
                ++diag_start;
            } else {
                size_t i0 = i;
                size_t j0 = j;
                KeyT val = a;
                while (i < left_n && left_keys[i] == val) {
                    ++i;
                }
                while (j < right_n && right_keys[j] == val) {
                    ++j;
                }
                size_t ilen = i - i0;
                size_t jlen = j - j0;
                for (size_t ii = 0; ii < ilen; ++ii) {
                    int64_t lid = left_rowids ? left_rowids[i0 + ii] : static_cast<int64_t>(i0 + ii);
                    for (size_t jj = 0; jj < jlen; ++jj) {
                        int64_t rid = right_rowids ? right_rowids[j0 + jj] : static_cast<int64_t>(j0 + jj);
                        out_left[write_pos] = lid;
                        out_right[write_pos] = rid;
                        ++write_pos;
                    }
                }
                diag_start += ilen + jlen;
            }
        }
    }

} // namespace gpu

template <typename KeyT>
JoinResult sortMergeJoin(const JoinColumn<KeyT>& left, const JoinColumn<KeyT>& right, cudaStream_t stream)
{
    int writer_threads = 256;
    size_t total_count = left.row_count + right.row_count;
    size_t writer_blocks = DIV_UP(total_count, writer_threads);
    if (writer_blocks < 1) {
        writer_blocks = 1;
    }
    if (writer_blocks > 65535) {
        writer_blocks = 65535;
    }

    size_t* d_partition_counts = nullptr;
    size_t* d_partition_offsets = nullptr;
    CHECKED_CALL_THROW(cudaMalloc(&d_partition_counts, writer_blocks * sizeof(size_t)));
    CHECKED_CALL_THROW(cudaMalloc(&d_partition_offsets, writer_blocks * sizeof(size_t)));

    int count_threads = 128;
    int count_blocks = DIV_UP(writer_blocks, count_threads);
    gpu::countPairsKernel<KeyT><<<count_blocks, count_threads, 0, stream>>>(left.keys,
                                                                            left.row_count,
                                                                            right.keys,
                                                                            right.row_count,
                                                                            d_partition_counts,
                                                                            writer_blocks);
    CHECKED_CALL_THROW(cudaStreamSynchronize(stream));

    size_t* h_counts = nullptr;
    size_t* h_offsets = nullptr;
    CHECKED_CALL_THROW(cudaMallocHost(&h_counts, writer_blocks * sizeof(size_t)));
    CHECKED_CALL_THROW(cudaMallocHost(&h_offsets, writer_blocks * sizeof(size_t)));
    CHECKED_CALL_THROW(
        cudaMemcpy(h_counts, d_partition_counts, writer_blocks * sizeof(size_t), cudaMemcpyDeviceToHost));
    size_t total_pairs = 0;
    for (size_t bi = 0; bi < writer_blocks; ++bi) {
        h_offsets[bi] = total_pairs;
        total_pairs += h_counts[bi];
    }
    CHECKED_CALL_THROW(
        cudaMemcpy(d_partition_offsets, h_offsets, writer_blocks * sizeof(size_t), cudaMemcpyHostToDevice));
    CHECKED_CALL_THROW(cudaFreeHost(h_counts));
    CHECKED_CALL_THROW(cudaFreeHost(h_offsets));

    int64_t* d_out_left = nullptr;
    int64_t* d_out_right = nullptr;
    CHECKED_CALL_THROW(cudaMalloc(&d_out_left, total_pairs * sizeof(int64_t)));
    CHECKED_CALL_THROW(cudaMalloc(&d_out_right, total_pairs * sizeof(int64_t)));
    gpu::writePairsKernel<KeyT><<<writer_blocks, writer_threads, 0, stream>>>(left.keys,
                                                                              left.row_count,
                                                                              left.rowids,
                                                                              right.keys,
                                                                              right.row_count,
                                                                              right.rowids,
                                                                              d_out_left,
                                                                              d_out_right,
                                                                              d_partition_offsets,
                                                                              writer_blocks);
    CHECKED_CALL_THROW(cudaStreamSynchronize(stream));
    CHECKED_CALL_THROW(cudaFree(d_partition_counts));
    CHECKED_CALL_THROW(cudaFree(d_partition_offsets));
    return JoinResult { d_out_left, d_out_right, total_pairs };
}

#define X(name, DT, VT)                                                                                                \
    template JoinResult sortMergeJoin<DT>(const JoinColumn<DT>&, const JoinColumn<DT>&, cudaStream_t);
LIST_TYPES(X)
#undef X

} // namespace velodb
