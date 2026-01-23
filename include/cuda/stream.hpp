#pragma once

#include "common/result.hpp"

#include <cuda_runtime.h>

namespace velodb {

class CudaStream {
public:
    CudaStream();
    explicit CudaStream(unsigned int flags);
    ~CudaStream();

    // Non-copyable but movable
    CudaStream(const CudaStream&) = delete;
    CudaStream& operator=(const CudaStream&) = delete;
    CudaStream(CudaStream&& other) noexcept;
    CudaStream& operator=(CudaStream&& other) noexcept;

    cudaStream_t get() const { return stream_; }
    bool isValid() const { return stream_ != nullptr; }

    // Stream operations
    Result<void> synchronize();

private:
    cudaStream_t stream_;
};

} // namespace velodb
