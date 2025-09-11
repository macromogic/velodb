#include "cuda/event_pool.hpp"

#include "cuda/event.hpp"
#include "cuda/helper.hpp"

#include <algorithm>

namespace velodb {

// EventHandle implementation
EventPool::EventHandle::EventHandle(EventPool* pool, std::unique_ptr<CudaEvent> event)
    : pool_(pool)
    , event_(std::move(event))
{
}

EventPool::EventHandle::~EventHandle()
{
    release();
}

EventPool::EventHandle::EventHandle(EventHandle&& other) noexcept
    : pool_(other.pool_)
    , event_(std::move(other.event_))
{
    other.pool_ = nullptr;
}

EventPool::EventHandle& EventPool::EventHandle::operator=(EventHandle&& other) noexcept
{
    if (this != &other) {
        release();
        pool_ = other.pool_;
        event_ = std::move(other.event_);
        other.pool_ = nullptr;
    }
    return *this;
}

void EventPool::EventHandle::release()
{
    if (pool_ && event_) {
        pool_->returnEvent(std::move(event_));
        pool_ = nullptr;
        event_.reset(); // Explicitly reset the unique_ptr
    }
}

// EventPool implementation
EventPool::EventPool(size_t initial_size, size_t max_size, unsigned int event_flags)
    : total_count_(0)
    , max_size_(max_size)
    , default_flags_(event_flags)
    , destroyed_(false)
{
    // Pre-allocate initial events
    for (size_t i = 0; i < initial_size; ++i) {
        auto result = createEvent();
        if (result && result.value() && result.value()->isValid()) {
            available_events_.push_back(std::move(result.value()));
            ++total_count_;
        }
    }
}

EventPool::~EventPool()
{
    std::lock_guard<std::mutex> lock(mutex_);
    destroyed_ = true;
    clear();
}

Result<EventPool::EventHandle> EventPool::acquire()
{
    return acquire(default_flags_);
}

Result<EventPool::EventHandle> EventPool::acquire(unsigned int flags)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (destroyed_) {
        return Result<EventHandle>::failure("EventPool has been destroyed");
    }

    std::unique_ptr<CudaEvent> event;

    // Try to reuse an existing event with matching flags
    // Clean up any invalid events while searching
    while (!available_events_.empty()) {
        auto& back_event = available_events_.back();
        if (back_event && back_event->isValid() && back_event->getFlags() == flags) {
            // Found a compatible event
            event = std::move(back_event);
            available_events_.pop_back();
            break;
        } else {
            // Remove invalid or incompatible event
            available_events_.pop_back();
            if (!back_event || !back_event->isValid()) {
                // This was an invalid event, decrement count
                if (total_count_ > 0) {
                    --total_count_;
                }
            }
        }
    }

    // If no compatible event found, create a new one (if under limit)
    if (!event) {
        if (max_size_ == 0 || total_count_ < max_size_) {
            auto result = createEvent(flags);
            if (!result) {
                return Result<EventHandle>::failure(result.error());
            }
            event = std::move(result.value());
            ++total_count_;
        } else {
            return Result<EventHandle>::failure("EventPool has reached maximum size");
        }
    }

    return Result<EventHandle>::success(EventHandle(this, std::move(event)));
}

size_t EventPool::availableCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return available_events_.size();
}

size_t EventPool::totalCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return total_count_;
}

Result<void> EventPool::synchronizeAll()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (destroyed_) {
        return Result<void>::failure("EventPool has been destroyed");
    }

    for (auto& event : available_events_) {
        if (event && event->isValid()) {
            auto result = event->synchronize();
            if (!result) {
                return result;
            }
        }
    }

    return Result<void>::success();
}

void EventPool::clear()
{
    available_events_.clear();
    total_count_ = 0;
}

EventPool& EventPool::instance()
{
    static EventPool global_pool(8, 32, cudaEventDefault);
    return global_pool;
}

EventPool& EventPool::timingInstance()
{
    static EventPool timing_pool(4, 16, cudaEventDefault);
    return timing_pool;
}

void EventPool::returnEvent(std::unique_ptr<CudaEvent> event)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (destroyed_ || !event || !event->isValid()) {
        return;
    }

    available_events_.push_back(std::move(event));
}

Result<std::unique_ptr<CudaEvent>> EventPool::createEvent()
{
    return createEvent(default_flags_);
}

Result<std::unique_ptr<CudaEvent>> EventPool::createEvent(unsigned int flags)
{
    try {
        auto event = std::make_unique<CudaEvent>(flags);
        return Result<std::unique_ptr<CudaEvent>>::success(std::move(event));
    } catch (const std::exception& e) {
        return Result<std::unique_ptr<CudaEvent>>::failure(e.what());
    }
}

// EventGuard implementation
EventGuard::EventGuard(EventPool& pool)
{
    auto result = pool.acquire();
    if (result) {
        handle_ = std::move(result.value());
    }
}

EventGuard::EventGuard(unsigned int flags, EventPool& pool)
{
    auto result = pool.acquire(flags);
    if (result) {
        handle_ = std::move(result.value());
    }
}

EventGuard::~EventGuard()
{
    release();
}

EventGuard::EventGuard(EventGuard&& other) noexcept
    : handle_(std::move(other.handle_))
{
}

EventGuard& EventGuard::operator=(EventGuard&& other) noexcept
{
    if (this != &other) {
        release();
        handle_ = std::move(other.handle_);
    }
    return *this;
}

CudaEvent* EventGuard::get() const
{
    return handle_.get();
}

CudaEvent* EventGuard::operator->() const
{
    return handle_.get();
}

CudaEvent& EventGuard::operator*() const
{
    return *handle_.get();
}

bool EventGuard::isValid() const
{
    return handle_.isValid();
}

void EventGuard::release()
{
    handle_.release();
}

// TimingGuard implementation
TimingGuard::TimingGuard(EventPool& pool)
    : started_(false)
    , stopped_(false)
{
    auto start_result = pool.acquire();
    auto end_result = pool.acquire();

    if (start_result && end_result) {
        start_event_ = std::move(start_result.value());
        end_event_ = std::move(end_result.value());
    }
}

TimingGuard::~TimingGuard()
{
    // Events are automatically returned to pool by EventHandle destructors
}

TimingGuard::TimingGuard(TimingGuard&& other) noexcept
    : start_event_(std::move(other.start_event_))
    , end_event_(std::move(other.end_event_))
    , started_(other.started_)
    , stopped_(other.stopped_)
{
    other.started_ = false;
    other.stopped_ = false;
}

TimingGuard& TimingGuard::operator=(TimingGuard&& other) noexcept
{
    if (this != &other) {
        start_event_ = std::move(other.start_event_);
        end_event_ = std::move(other.end_event_);
        started_ = other.started_;
        stopped_ = other.stopped_;
        other.started_ = false;
        other.stopped_ = false;
    }
    return *this;
}

Result<void> TimingGuard::start(cudaStream_t stream)
{
    if (!isValid()) {
        return Result<void>::failure("Invalid TimingGuard - events not acquired");
    }

    auto result = start_event_->record(stream);
    if (result) {
        started_ = true;
        stopped_ = false;
    }
    return result;
}

Result<void> TimingGuard::stop(cudaStream_t stream)
{
    if (!isValid()) {
        return Result<void>::failure("Invalid TimingGuard - events not acquired");
    }

    if (!started_) {
        return Result<void>::failure("TimingGuard not started - call start() first");
    }

    auto result = end_event_->record(stream);
    if (result) {
        stopped_ = true;
    }
    return result;
}

Result<float> TimingGuard::elapsedTime()
{
    if (!isValid()) {
        return Result<float>::failure("Invalid TimingGuard - events not acquired");
    }

    if (!started_) {
        return Result<float>::failure("TimingGuard not started - call start() first");
    }

    if (!stopped_) {
        return Result<float>::failure("TimingGuard not stopped - call stop() first");
    }

    // Synchronize end event to ensure timing is complete
    auto sync_result = end_event_->synchronize();
    if (!sync_result) {
        return Result<float>::failure("Failed to synchronize end event: " + sync_result.error());
    }

    return CudaEvent::elapsedTime(*start_event_, *end_event_);
}

bool TimingGuard::isValid() const
{
    return start_event_.isValid() && end_event_.isValid();
}

} // namespace velodb
