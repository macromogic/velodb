#pragma once

#include "common/exception.hpp"
#include "cuda/helper.hpp"
#include "data/data_location.hpp"

#include <cuda_runtime.h>

namespace velodb {

class MemoryAllocator {
public:
    MemoryAllocator()
    {
        cudaMemPoolProps poolProps = {};
        poolProps.allocType = cudaMemAllocationTypePinned;
        poolProps.handleTypes = cudaMemHandleTypeNone;
        poolProps.location = { .type = cudaMemLocationTypeHost, .id = 0 };
        cudaMemPoolCreate(&host_pool_, &poolProps);
    }

    ~MemoryAllocator() { cudaMemPoolDestroy(host_pool_); }

    template <typename T>
    static T* allocate(DataLocation location, size_t n, cudaStream_t stream = 0)
    {
        return static_cast<T*>(getInstance().allocateImpl(location, n * sizeof(T), stream));
    }

    static void deallocate(void* ptr, cudaStream_t stream = 0) { getInstance().deallocateImpl(ptr, stream); }

private:
    void* allocateImpl(DataLocation location, size_t size, cudaStream_t stream = 0)
    {
        void* ptr = nullptr;
        switch (location) {
        case DataLocation::HOST: {
            CHECKED_CALL_THROW(cudaMallocFromPoolAsync(&ptr, size, host_pool_, stream));
            break;
        }
        case DataLocation::CUDA: {
            CHECKED_CALL_THROW(cudaMallocAsync(&ptr, size, stream));
            break;
        }
        default:
            VELODB_THROW(ExecutionError, "Unsupported data location for allocation");
        }
        return ptr;
    }

    void deallocateImpl(void* ptr, cudaStream_t stream = 0) { CHECKED_CALL_THROW(cudaFreeAsync(ptr, stream)); }

    static MemoryAllocator& getInstance()
    {
        static MemoryAllocator instance;
        return instance;
    }

    cudaMemPool_t host_pool_;
};

}
