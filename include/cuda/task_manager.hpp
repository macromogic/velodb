#pragma once

#include "common/copy_traits.hpp"
#include "cuda/commands.hpp"

#include <memory>

#include <cuda_runtime.h>

namespace velodb {

class TaskManager : private NonCopyable {
public:
    explicit TaskManager(size_t queue_capacity = 1024);

    TaskManager(TaskManager&& other) noexcept = default;
    TaskManager& operator=(TaskManager&& other) noexcept = default;

    ~TaskManager();

    bool start();
    bool stop(uint32_t timeout_ms = 5000);
    bool isRunning() const { return is_running_; }
    uint64_t submitCommand(Command& cmd) { return queue_ctrl_.push(cmd); }
    void waitCommand(uint64_t wait_id) { queue_ctrl_.wait(wait_id); }

private:
    CommandQueueController queue_ctrl_;
    bool is_running_;
};

} // namespace velodb
