#include "cuda/stream.hpp"

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

} // namespace velodb
