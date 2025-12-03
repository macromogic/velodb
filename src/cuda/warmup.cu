#include "cuda/helper.hpp"
#include "cuda/warmup.hpp"

#include <cstdint>

#include <cuda_runtime.h>
#include <device_types.h>
#include <driver_types.h>

namespace velodb {

namespace gpu {

    __global__ void warmup_kernel(uint32_t* glb, int n)
    {
        float acc = threadIdx.x * 1.0f;
#pragma unroll 16
        for (int i = 0; i < 32; ++i)
            acc = sinf(acc);

        int idx = blockIdx.x * blockDim.x + threadIdx.x;
        if (idx < n)
            glb[idx] ^= 0x5a;

        extern __shared__ uint32_t sm[];
        sm[threadIdx.x] = blockIdx.x;
        __syncthreads();
        if (threadIdx.x == 0)
            atomicAdd(&glb[0], sm[0]);
    }

} // namespace gpu

Result<void> runtime_warmup()
{
    // 1. Context initialization
    CHECKED_CALL(cudaFree(0));

    // 2. Create non-blocking stream
    cudaStream_t s;
    CHECKED_CALL(cudaStreamCreateWithFlags(&s, cudaStreamNonBlocking));

    // 3. Prepare buffer for warmup
    const auto bytes = 4096;
    uint32_t* d_buf = nullptr;
    CHECKED_CALL(cudaMalloc(&d_buf, bytes));
    CHECKED_CALL(cudaMemsetAsync(d_buf, 0, bytes, s));

    // 4. Launch warmup kernel
    dim3 block(128);
    dim3 grid(DIV_UP(bytes / sizeof(uint32_t), block.x));
    gpu::warmup_kernel<<<grid, block, 4096, s>>>(d_buf, bytes / sizeof(uint32_t));

    // 5. Synchronize and clean up
    CHECKED_CALL(cudaStreamSynchronize(s));
    CHECKED_CALL(cudaFree(d_buf));
    CHECKED_CALL(cudaStreamDestroy(s));

    return Result<void>::success();
}

} // namespace velodb
