#include "cuda/stream.hpp"

#include "cuda/event.hpp"
#include "cuda/helper.hpp"

namespace velodb {

CudaStream::CudaStream()
    : stream_(nullptr)
{
    CHECKED_CALL_THROW(cudaStreamCreate(&stream_));
}

CudaStream::CudaStream(unsigned int flags)
    : stream_(nullptr)
{
    CHECKED_CALL_THROW(cudaStreamCreateWithFlags(&stream_, flags));
}

CudaStream::~CudaStream()
{
    if (stream_ != nullptr) {
        cudaStreamDestroy(stream_);
    }
}

CudaStream::CudaStream(CudaStream&& other) noexcept
    : stream_(other.stream_)
{
    other.stream_ = nullptr;
}

CudaStream& CudaStream::operator=(CudaStream&& other) noexcept
{
    if (this != &other) {
        if (stream_ != nullptr) {
            cudaStreamDestroy(stream_);
        }

        stream_ = other.stream_;
        other.stream_ = nullptr;
    }
    return *this;
}

Result<void> CudaStream::synchronize()
{
    if (!isValid()) {
        return Result<void>::failure("Invalid CUDA stream");
    }

    CHECKED_CALL(cudaStreamSynchronize(stream_));

    return Result<void>::success();
}

Result<void> CudaStream::recordEvent(cudaEvent_t event)
{
    if (!isValid()) {
        return Result<void>::failure("Invalid CUDA stream");
    }

    CHECKED_CALL(cudaEventRecord(event, stream_));

    return Result<void>::success();
}

Result<void> CudaStream::recordEvent(CudaEvent& event)
{
    if (!isValid()) {
        return Result<void>::failure("Invalid CUDA stream");
    }

    if (!event.isValid()) {
        return Result<void>::failure("Invalid CUDA event");
    }

    CHECKED_CALL(cudaEventRecord(event.get(), stream_));

    return Result<void>::success();
}

Result<void> CudaStream::waitEvent(cudaEvent_t event)
{
    if (!isValid()) {
        return Result<void>::failure("Invalid CUDA stream");
    }

    CHECKED_CALL(cudaStreamWaitEvent(stream_, event, 0));

    return Result<void>::success();
}

Result<void> CudaStream::waitEvent(const CudaEvent& event)
{
    if (!isValid()) {
        return Result<void>::failure("Invalid CUDA stream");
    }

    if (!event.isValid()) {
        return Result<void>::failure("Invalid CUDA event");
    }

    CHECKED_CALL(cudaStreamWaitEvent(stream_, event.get(), 0));

    return Result<void>::success();
}

CudaStream& CudaStream::getH2DStream()
{
    static CudaStream h2d_stream(cudaStreamNonBlocking);
    return h2d_stream;
}

CudaStream& CudaStream::getD2HStream()
{
    static CudaStream d2h_stream(cudaStreamNonBlocking);
    return d2h_stream;
}

CudaStream& CudaStream::getDummyStream()
{
    static CudaStream dummy_stream(cudaStreamNonBlocking);
    return dummy_stream;
}

} // namespace velodb
