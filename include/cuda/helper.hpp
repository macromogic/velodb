#pragma once

#include "common/exception.hpp"
#include "common/fmt.hpp"
#include "common/result.hpp"

#include <fmt/format.h>

#include <cuda_runtime.h>

namespace velodb {

#define CHECKED_CALL(call)                                                                                             \
    do {                                                                                                               \
        cudaError_t err = (call);                                                                                      \
        if (err != cudaSuccess) {                                                                                      \
            return Result<void>::failure(cudaGetErrorString(err));                                                     \
        }                                                                                                              \
    } while (0)

#define CHECKED_CALL_T(T, call)                                                                                        \
    do {                                                                                                               \
        cudaError_t err = (call);                                                                                      \
        if (err != cudaSuccess) {                                                                                      \
            return Result<T>::failure(cudaGetErrorString(err));                                                        \
        }                                                                                                              \
    } while (0)

#define CHECKED_CALL_THROW(call)                                                                                       \
    do {                                                                                                               \
        cudaError_t err = (call);                                                                                      \
        if (err != cudaSuccess) {                                                                                      \
            VELODB_THROW(ExecutionError, fmt::format("CUDA error: {}", cudaGetErrorString(err)));                      \
        }                                                                                                              \
    } while (0)

} // namespace velodb
