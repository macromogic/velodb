#pragma once

namespace velodb {

enum class DataLocation {
    VIEW, // Non-owning reference to HOST data
    CUDA_VIEW, // Non-owning reference to CUDA data
    HOST_PINNED, // Pinned memory (page-locked), fast DMA transfers
    HOST_PAGEABLE, // Regular pageable memory, needs staging buffer for transfers
    CUDA,

    // Backward compatibility alias
    HOST = HOST_PINNED,
};

inline auto format_as(DataLocation location)
{
    switch (location) {
    case DataLocation::HOST_PINNED:
        return "HOST_PINNED";
    case DataLocation::HOST_PAGEABLE:
        return "HOST_PAGEABLE";
    case DataLocation::CUDA:
        return "CUDA";
    case DataLocation::VIEW:
        return "VIEW";
    case DataLocation::CUDA_VIEW:
        return "CUDA_VIEW";
    default:
        return "UNKNOWN";
    }
}

inline bool isHostLocation(DataLocation location)
{
    return location == DataLocation::HOST_PINNED || location == DataLocation::HOST_PAGEABLE;
}

inline bool isCudaLocation(DataLocation location)
{
    return location == DataLocation::CUDA || location == DataLocation::CUDA_VIEW;
}

inline bool isViewLocation(DataLocation location)
{
    return location == DataLocation::VIEW || location == DataLocation::CUDA_VIEW;
}

} // namespace velodb
