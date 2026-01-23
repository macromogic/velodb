#pragma once

#include "cuda/commands.hpp"

namespace velodb::cuda {

__global__ void persistentKernel(CommandQueue queue);

} // namespace velodb::cuda
