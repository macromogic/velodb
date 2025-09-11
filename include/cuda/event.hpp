#pragma once

#include "common/copy_traits.hpp"
#include "common/result.hpp"

#include <cuda_runtime.h>

namespace velodb {

class CudaStream; // Forward declaration

class CudaEvent : private NonCopyable {
public:
    CudaEvent();
    explicit CudaEvent(unsigned int flags);
    ~CudaEvent();

    CudaEvent(CudaEvent&& other) noexcept;
    CudaEvent& operator=(CudaEvent&& other) noexcept;

    cudaEvent_t get() const { return event_; }
    bool isValid() const { return event_ != nullptr; }
    Result<void> record();
    Result<void> record(cudaStream_t stream);
    Result<void> record(const CudaStream& stream);
    Result<void> markCompleted();
    Result<void> synchronize();
    Result<bool> query();
    static Result<float> elapsedTime(const CudaEvent& start_event, const CudaEvent& end_event);
    unsigned int getFlags() const { return flags_; }

private:
    cudaEvent_t event_;
    unsigned int flags_;
};

} // namespace velodb
