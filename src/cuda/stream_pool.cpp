#include "cuda/stream_pool.hpp"

#include "cuda/stream.hpp"

namespace velodb {

// StreamPool::StreamHandle implementation

StreamPool::StreamHandle::StreamHandle(StreamPool* pool, std::unique_ptr<CudaStream> stream)
    : pool_(pool)
    , stream_(std::move(stream))
{
}

StreamPool::StreamHandle::~StreamHandle()
{
    release();
}

StreamPool::StreamHandle::StreamHandle(StreamHandle&& other) noexcept
    : pool_(other.pool_)
    , stream_(std::move(other.stream_))
{
    other.pool_ = nullptr;
}

StreamPool::StreamHandle& StreamPool::StreamHandle::operator=(StreamHandle&& other) noexcept
{
    if (this != &other) {
        release();
        pool_ = other.pool_;
        stream_ = std::move(other.stream_);
        other.pool_ = nullptr;
    }
    return *this;
}

void StreamPool::StreamHandle::release()
{
    if (pool_ && stream_) {
        pool_->return_stream(std::move(stream_));
        pool_ = nullptr;
    }
}

// StreamPool implementation

StreamPool::StreamPool(size_t initial_size, size_t max_size)
    : total_count_(0)
    , max_size_(max_size)
    , destroyed_(false)
{

    // Pre-create initial streams
    for (size_t i = 0; i < initial_size; ++i) {
        auto stream_result = create_stream();
        if (stream_result) {
            available_streams_.push(std::move(stream_result.value()));
            ++total_count_;
        }
    }
}

StreamPool::~StreamPool()
{
    std::lock_guard<std::mutex> lock(mutex_);
    destroyed_ = true;
    clear();
}

Result<StreamPool::StreamHandle> StreamPool::acquire_stream()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (destroyed_) {
        return Result<StreamHandle>::failure("StreamPool has been destroyed");
    }

    std::unique_ptr<CudaStream> stream;

    if (!available_streams_.empty()) {
        // Reuse existing stream
        stream = std::move(available_streams_.front());
        available_streams_.pop();
    } else if (max_size_ == 0 || total_count_ < max_size_) {
        // Create new stream
        auto stream_result = create_stream();
        if (!stream_result) {
            return Result<StreamHandle>::failure(stream_result.error());
        }
        stream = std::move(stream_result.value());
        ++total_count_;
    } else {
        return Result<StreamHandle>::failure("StreamPool is at maximum capacity");
    }

    return Result<StreamHandle>::success(StreamHandle(this, std::move(stream)));
}

void StreamPool::return_stream(std::unique_ptr<CudaStream> stream)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!destroyed_ && stream && stream->is_valid()) {
        available_streams_.push(std::move(stream));
    } else {
        // Stream is invalid or pool is destroyed, decrease total count
        if (total_count_ > 0) {
            --total_count_;
        }
    }
}

size_t StreamPool::available_count() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return available_streams_.size();
}

size_t StreamPool::total_count() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return total_count_;
}

Result<void> StreamPool::synchronize_all()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (destroyed_) {
        return Result<void>::failure("StreamPool has been destroyed");
    }

    // Create a temporary queue to iterate through all streams
    std::queue<std::unique_ptr<CudaStream>> temp_queue;

    while (!available_streams_.empty()) {
        auto& stream = available_streams_.front();
        auto sync_result = stream->synchronize();
        if (!sync_result) {
            // Put back all streams we've processed
            while (!temp_queue.empty()) {
                available_streams_.push(std::move(temp_queue.front()));
                temp_queue.pop();
            }
            return sync_result;
        }

        temp_queue.push(std::move(available_streams_.front()));
        available_streams_.pop();
    }

    // Put all streams back
    while (!temp_queue.empty()) {
        available_streams_.push(std::move(temp_queue.front()));
        temp_queue.pop();
    }

    return Result<void>::success();
}

void StreamPool::clear()
{
    // Note: This assumes mutex is already locked by caller
    while (!available_streams_.empty()) {
        available_streams_.pop();
    }
    total_count_ = 0;
}

StreamPool& StreamPool::instance()
{
    static StreamPool global_pool;
    return global_pool;
}

Result<std::unique_ptr<CudaStream>> StreamPool::create_stream()
{
    auto stream = std::make_unique<CudaStream>(cudaStreamNonBlocking);
    if (!stream->is_valid()) {
        return Result<std::unique_ptr<CudaStream>>::failure("Failed to create CUDA stream");
    }
    return Result<std::unique_ptr<CudaStream>>::success(std::move(stream));
}

// StreamGuard implementation

StreamGuard::StreamGuard(StreamPool& pool)
{
    auto handle_result = pool.acquire_stream();
    if (handle_result) {
        handle_ = std::move(handle_result.value());
    }
    // If acquisition fails, handle_ will be invalid
}

StreamGuard::~StreamGuard()
{
    // StreamHandle destructor automatically releases the stream
}

StreamGuard::StreamGuard(StreamGuard&& other) noexcept
    : handle_(std::move(other.handle_))
{
}

StreamGuard& StreamGuard::operator=(StreamGuard&& other) noexcept
{
    if (this != &other) {
        handle_ = std::move(other.handle_);
    }
    return *this;
}

CudaStream* StreamGuard::get() const
{
    return handle_.get();
}

CudaStream* StreamGuard::operator->() const
{
    return handle_.get();
}

CudaStream& StreamGuard::operator*() const
{
    return *handle_;
}

bool StreamGuard::is_valid() const
{
    return handle_.is_valid();
}

void StreamGuard::release()
{
    handle_.release();
}

} // namespace velodb
