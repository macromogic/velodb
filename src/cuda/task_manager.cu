#include "cuda/helper.hpp"
#include "cuda/task_manager.hpp"
#include "data/type_traits.hpp"

#include "cuda/persistent_kernel.cuh"

#include <algorithm>
#include <cstring>
#include <thread>

namespace velodb {

TaskManager::TaskManager(size_t queue_capacity)
    : queue_ctrl_(queue_capacity)
    , is_running_(false)
{
}

TaskManager::~TaskManager()
{
    if (is_running_) {
        stop(5000);
    }
}

bool TaskManager::start()
{
    if (is_running_) {
        return false;
    }

    int block_size = 256;
    int shmem_size = 48 * (1 << 10);
    CHECKED_CALL_THROW(
        cudaFuncSetAttribute(cuda::persistentKernel, cudaFuncAttributeMaxDynamicSharedMemorySize, shmem_size));
    CHECKED_CALL_THROW(
        cudaFuncSetAttribute(cuda::persistentKernel, cudaFuncAttributeNonPortableClusterSizeAllowed, 16));

    int device;
    cudaDeviceProp prop;
    int num_blocks_per_sm;
    CHECKED_CALL_THROW(cudaGetDevice(&device));
    CHECKED_CALL_THROW(cudaGetDeviceProperties(&prop, device));
    CHECKED_CALL_THROW(cudaOccupancyMaxActiveBlocksPerMultiprocessor(&num_blocks_per_sm,
                                                                     cuda::persistentKernel,
                                                                     block_size,
                                                                     shmem_size));

    // Leave some SMs for system operations (like cudaMemcpy internal kernels) to avoid deadlock
    // If we occupy 100% of SMs, other kernels cannot launch.
    int reserved_sms = 4;
    int active_sms = std::max(1, prop.multiProcessorCount - reserved_sms);
    int num_blocks = active_sms * num_blocks_per_sm;

    fmt::println("Launching persistent kernel with {} blocks ({} SMs), block size {}, shared memory {} bytes",
                 num_blocks,
                 active_sms,
                 block_size,
                 shmem_size);
    CommandQueue queue = queue_ctrl_.getQueue();
    void* kernel_args[] = { &queue };
    CHECKED_CALL_THROW(cudaLaunchCooperativeKernel(cuda::persistentKernel,
                                                   num_blocks,
                                                   block_size,
                                                   kernel_args,
                                                   shmem_size,
                                                   queue_ctrl_.getStream()));
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        fmt::println(stderr, "Failed to launch persistent kernel: {}", cudaGetErrorString(err));
        return false;
    }

    is_running_ = true;
    return true;
}

bool TaskManager::stop(uint32_t timeout_ms)
{
    if (!is_running_) {
        fmt::println("TaskManager is not running.");
        return true;
    }
    fmt::println("Stopping TaskManager...");

    // Send terminate command
    Command terminate_cmd {};
    terminate_cmd.opcode = OpCode::OP_TERMINATE;
    queue_ctrl_.push(terminate_cmd);

    // Wait for kernel to exit
    auto start = std::chrono::steady_clock::now();

    cudaStream_t stream = queue_ctrl_.getStream();
    while (true) {
        cudaError_t err = cudaStreamQuery(stream);

        if (err == cudaSuccess) {
            // Kernel has completed
            is_running_ = false;
            return true;
        } else if (err != cudaErrorNotReady) {
            // Error occurred
            fmt::println(stderr, "Error while waiting for TaskManager to stop: {}", cudaGetErrorString(err));
            return false;
        }

        // Check timeout
        if (timeout_ms > 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
            if (elapsed.count() >= timeout_ms) {
                fmt::println("Timeout while waiting for TaskManager to stop. Forcing stop.");
                cudaDeviceReset();
                is_running_ = false;
                return false;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

} // namespace velodb
