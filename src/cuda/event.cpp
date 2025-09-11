#include "cuda/event.hpp"

#include "cuda/helper.hpp"
#include "cuda/stream.hpp"

namespace velodb {

CudaEvent::CudaEvent()
    : event_(nullptr)
    , flags_(cudaEventDefault)
{
    CHECKED_CALL_THROW(cudaEventCreate(&event_));
}

CudaEvent::CudaEvent(unsigned int flags)
    : event_(nullptr)
    , flags_(flags)
{
    CHECKED_CALL_THROW(cudaEventCreateWithFlags(&event_, flags));
}

CudaEvent::~CudaEvent()
{
    if (event_ != nullptr) {
        cudaEventDestroy(event_);
    }
}

CudaEvent::CudaEvent(CudaEvent&& other) noexcept
    : event_(other.event_)
    , flags_(other.flags_)
{
    other.event_ = nullptr;
    other.flags_ = 0;
}

CudaEvent& CudaEvent::operator=(CudaEvent&& other) noexcept
{
    if (this != &other) {
        if (event_ != nullptr) {
            cudaEventDestroy(event_);
        }

        event_ = other.event_;
        flags_ = other.flags_;
        other.event_ = nullptr;
        other.flags_ = 0;
    }
    return *this;
}

Result<void> CudaEvent::record()
{
    if (!isValid()) {
        return Result<void>::failure("Invalid CUDA event");
    }

    CHECKED_CALL(cudaEventRecord(event_, nullptr));

    return Result<void>::success();
}

Result<void> CudaEvent::record(cudaStream_t stream)
{
    if (!isValid()) {
        return Result<void>::failure("Invalid CUDA event");
    }

    CHECKED_CALL(cudaEventRecord(event_, stream));

    return Result<void>::success();
}

Result<void> CudaEvent::record(const CudaStream& stream)
{
    if (!isValid()) {
        return Result<void>::failure("Invalid CUDA event");
    }

    if (!stream.isValid()) {
        return Result<void>::failure("Invalid CUDA stream");
    }

    CHECKED_CALL(cudaEventRecord(event_, stream.get()));

    return Result<void>::success();
}

Result<void> CudaEvent::markCompleted()
{
    if (!isValid()) {
        return Result<void>::failure("Invalid CUDA event");
    }

    CHECKED_CALL(cudaEventRecord(event_, CudaStream::getDummyStream().get()));

    return Result<void>::success();
}

Result<void> CudaEvent::synchronize()
{
    if (!isValid()) {
        return Result<void>::failure("Invalid CUDA event");
    }

    CHECKED_CALL(cudaEventSynchronize(event_));

    return Result<void>::success();
}

Result<bool> CudaEvent::query()
{
    if (!isValid()) {
        return Result<bool>::failure("Invalid CUDA event");
    }

    cudaError_t err = cudaEventQuery(event_);
    if (err == cudaSuccess) {
        return Result<bool>::success(true);
    } else if (err == cudaErrorNotReady) {
        return Result<bool>::success(false);
    } else {
        return Result<bool>::failure(cudaGetErrorString(err));
    }
}

Result<float> CudaEvent::elapsedTime(const CudaEvent& start_event, const CudaEvent& end_event)
{
    if (!start_event.isValid()) {
        return Result<float>::failure("Invalid start event");
    }

    if (!end_event.isValid()) {
        return Result<float>::failure("Invalid end event");
    }

    float elapsed_ms = 0.0f;
    CHECKED_CALL_T(float, cudaEventElapsedTime(&elapsed_ms, start_event.get(), end_event.get()));

    return Result<float>::success(elapsed_ms);
}

} // namespace velodb
