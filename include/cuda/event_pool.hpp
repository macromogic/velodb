#pragma once

#include "common/copy_traits.hpp"
#include "common/result.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include <cuda_runtime.h>

namespace velodb {

class CudaEvent; // Forward declaration

class EventPool : private NonCopyable {
public:
    class EventHandle : private NonCopyable {
    public:
        EventHandle()
            : pool_(nullptr)
            , event_(nullptr)
        {
        }
        EventHandle(EventPool* pool, std::unique_ptr<CudaEvent> event);
        ~EventHandle();

        EventHandle(EventHandle&& other) noexcept;
        EventHandle& operator=(EventHandle&& other) noexcept;

        CudaEvent* get() const { return event_.get(); }
        CudaEvent* operator->() const { return event_.get(); }
        CudaEvent& operator*() const { return *event_; }

        bool isValid() const { return event_ != nullptr; }
        void release();

    private:
        EventPool* pool_;
        std::unique_ptr<CudaEvent> event_;
    };

    explicit EventPool(size_t initial_size = 8, size_t max_size = 32, unsigned int event_flags = 0);
    ~EventPool();

    EventPool(EventPool&&) = delete;
    EventPool& operator=(EventPool&&) = delete;

    Result<EventHandle> acquire();
    Result<EventHandle> acquire(unsigned int flags);
    size_t availableCount() const;
    size_t totalCount() const;
    Result<void> synchronizeAll();
    void clear();

    static EventPool& instance();
    static EventPool& timingInstance();

private:
    friend class EventHandle;

    void returnEvent(std::unique_ptr<CudaEvent> event);
    Result<std::unique_ptr<CudaEvent>> createEvent();
    Result<std::unique_ptr<CudaEvent>> createEvent(unsigned int flags);

    mutable std::mutex mutex_;
    std::vector<std::unique_ptr<CudaEvent>> available_events_;
    size_t total_count_;
    size_t max_size_;
    unsigned int default_flags_;
    bool destroyed_;
};

class EventGuard {
public:
    explicit EventGuard(EventPool& pool = EventPool::instance());
    explicit EventGuard(unsigned int flags, EventPool& pool = EventPool::instance());
    ~EventGuard();

    // Non-copyable but movable
    EventGuard(const EventGuard&) = delete;
    EventGuard& operator=(const EventGuard&) = delete;
    EventGuard(EventGuard&& other) noexcept;
    EventGuard& operator=(EventGuard&& other) noexcept;

    CudaEvent* get() const;
    CudaEvent* operator->() const;
    CudaEvent& operator*() const;

    bool isValid() const;
    void release();

private:
    EventPool::EventHandle handle_;
};

class TimingGuard {
public:
    explicit TimingGuard(EventPool& pool = EventPool::timingInstance());
    ~TimingGuard();

    // Non-copyable but movable
    TimingGuard(const TimingGuard&) = delete;
    TimingGuard& operator=(const TimingGuard&) = delete;
    TimingGuard(TimingGuard&& other) noexcept;
    TimingGuard& operator=(TimingGuard&& other) noexcept;

    Result<void> start(cudaStream_t stream = nullptr);
    Result<void> stop(cudaStream_t stream = nullptr);
    Result<float> elapsedTime();

    bool isValid() const;

private:
    EventPool::EventHandle start_event_;
    EventPool::EventHandle end_event_;
    bool started_;
    bool stopped_;
};

} // namespace velodb
