#pragma once

#include "common/result.hpp"

#include <cuda_runtime.h>

namespace velodb {

class CudaEvent; // Forward declaration

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
    Result<void> recordEvent(cudaEvent_t event);
    Result<void> recordEvent(CudaEvent& event);
    Result<void> waitEvent(cudaEvent_t event);
    Result<void> waitEvent(const CudaEvent& event);

    static CudaStream& getH2DStream();
    static CudaStream& getD2HStream();
    static CudaStream& getDummyStream();

private:
    cudaStream_t stream_;
};

} // namespace velodb
