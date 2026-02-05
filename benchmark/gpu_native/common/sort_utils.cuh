#pragma once

#include "types.hpp"

#include "launcher.cuh"

#include <thrust/device_ptr.h>
#include <thrust/execution_policy.h>
#include <thrust/gather.h>
#include <thrust/iterator/zip_iterator.h>
#include <thrust/sequence.h>
#include <thrust/sort.h>
#include <thrust/tuple.h>

namespace gpu_native {

// ============================================================================
// Sort Utilities using Thrust
// ============================================================================

// Sort indices by a single key (ascending)
template <typename KeyT>
inline void sort_indices_by_key(const KeyT* keys, int32_t* indices, size_t n, cudaStream_t stream = 0)
{
    // Create device pointers
    thrust::device_ptr<const KeyT> d_keys(keys);
    thrust::device_ptr<int32_t> d_indices(indices);

    // Initialize indices to [0, 1, 2, ...]
    thrust::sequence(thrust::cuda::par.on(stream), d_indices, d_indices + n);

    // Sort indices by keys
    thrust::sort_by_key(thrust::cuda::par.on(stream),
                        thrust::device_pointer_cast(const_cast<KeyT*>(keys)),
                        thrust::device_pointer_cast(const_cast<KeyT*>(keys)) + n,
                        d_indices);
}

// Sort indices by a single key (descending)
template <typename KeyT>
inline void sort_indices_by_key_desc(const KeyT* keys, int32_t* indices, size_t n, cudaStream_t stream = 0)
{
    thrust::device_ptr<const KeyT> d_keys(keys);
    thrust::device_ptr<int32_t> d_indices(indices);

    thrust::sequence(thrust::cuda::par.on(stream), d_indices, d_indices + n);

    thrust::sort_by_key(thrust::cuda::par.on(stream),
                        thrust::device_pointer_cast(const_cast<KeyT*>(keys)),
                        thrust::device_pointer_cast(const_cast<KeyT*>(keys)) + n,
                        d_indices,
                        thrust::greater<KeyT>());
}

// Gather elements using indices
template <typename T>
inline void gather_by_indices(const T* input, const int32_t* indices, T* output, size_t n, cudaStream_t stream = 0)
{
    thrust::device_ptr<const T> d_input(input);
    thrust::device_ptr<const int32_t> d_indices(indices);
    thrust::device_ptr<T> d_output(output);

    thrust::gather(thrust::cuda::par.on(stream), d_indices, d_indices + n, d_input, d_output);
}

// In-place reorder multiple arrays based on sorted indices
// This is a multi-column gather
template <typename... Arrays>
inline void reorder_by_indices(const int32_t* indices, size_t n, cudaStream_t stream, Arrays*... arrays)
{
    // For each array, gather into a temp buffer then copy back
    auto reorder_one = [&](auto* arr) {
        using T = std::remove_pointer_t<decltype(arr)>;
        T* temp;
        cudaMalloc(&temp, n * sizeof(T));
        gather_by_indices(arr, indices, temp, n, stream);
        cudaMemcpyAsync(arr, temp, n * sizeof(T), cudaMemcpyDeviceToDevice, stream);
        cudaFree(temp);
    };

    (reorder_one(arrays), ...);
}

// Sort result arrays by a key column (ascending), returns sorted count (for LIMIT)
template <typename KeyT>
inline uint32_t sort_results_by_key(KeyT* sort_key, size_t n, size_t limit, cudaStream_t stream, auto*... other_arrays)
{
    if (n == 0)
        return 0;

    // Allocate indices
    int32_t* indices;
    cudaMalloc(&indices, n * sizeof(int32_t));

    // Sort
    sort_indices_by_key(sort_key, indices, n, stream);

    // Reorder all arrays (including key)
    reorder_by_indices(indices, n, stream, sort_key, other_arrays...);

    cudaFree(indices);
    cudaStreamSynchronize(stream);

    return static_cast<uint32_t>(std::min(n, limit));
}

// Sort result arrays by a key column (descending), with optional secondary key
template <typename KeyT>
inline uint32_t sort_results_by_key_desc(KeyT* sort_key,
                                         size_t n,
                                         size_t limit,
                                         cudaStream_t stream,
                                         auto*... other_arrays)
{
    if (n == 0)
        return 0;

    int32_t* indices;
    cudaMalloc(&indices, n * sizeof(int32_t));

    sort_indices_by_key_desc(sort_key, indices, n, stream);
    reorder_by_indices(indices, n, stream, sort_key, other_arrays...);

    cudaFree(indices);
    cudaStreamSynchronize(stream);

    return static_cast<uint32_t>(std::min(n, limit));
}

// ============================================================================
// Two-key sort (primary DESC, secondary ASC) for Q18
// ============================================================================

struct Q18SortKey {
    double totalprice;
    int32_t orderdate;
};

struct Q18KeyComparator {
    __host__ __device__ bool operator()(const Q18SortKey& a, const Q18SortKey& b) const
    {
        if (a.totalprice != b.totalprice) {
            return a.totalprice > b.totalprice; // DESC
        }
        return a.orderdate < b.orderdate; // ASC
    }
};

inline void sort_q18_results(double* totalprice,
                             int32_t* orderdate,
                             int32_t* custkey,
                             int32_t* orderkey,
                             double* quantity,
                             size_t n,
                             cudaStream_t stream)
{
    if (n == 0)
        return;

    // Create composite keys
    Q18SortKey* keys;
    cudaMalloc(&keys, n * sizeof(Q18SortKey));

    // Copy to composite key (simple kernel)
    thrust::device_ptr<double> d_price(totalprice);
    thrust::device_ptr<int32_t> d_date(orderdate);
    thrust::device_ptr<Q18SortKey> d_keys(keys);

    thrust::transform(thrust::cuda::par.on(stream),
                      thrust::make_zip_iterator(thrust::make_tuple(d_price, d_date)),
                      thrust::make_zip_iterator(thrust::make_tuple(d_price + n, d_date + n)),
                      d_keys,
                      [] __device__(const thrust::tuple<double, int32_t>& t) {
                          Q18SortKey k;
                          k.totalprice = thrust::get<0>(t);
                          k.orderdate = thrust::get<1>(t);
                          return k;
                      });

    // Create indices
    int32_t* indices;
    cudaMalloc(&indices, n * sizeof(int32_t));
    thrust::device_ptr<int32_t> d_indices(indices);
    thrust::sequence(thrust::cuda::par.on(stream), d_indices, d_indices + n);

    // Sort
    thrust::sort_by_key(thrust::cuda::par.on(stream), d_keys, d_keys + n, d_indices, Q18KeyComparator());

    // Reorder all arrays
    reorder_by_indices(indices, n, stream, totalprice, orderdate, custkey, orderkey, quantity);

    cudaFree(keys);
    cudaFree(indices);
}

} // namespace gpu_native
