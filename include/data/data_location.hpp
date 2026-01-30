#pragma once

namespace velodb {

/**
 * @brief Specifies where data is stored.
 *
 * - VIEW: A non-owning reference to HOST data stored elsewhere
 * - CUDA_VIEW: A non-owning reference to CUDA data stored elsewhere
 * - HOST_PINNED: Pinned (page-locked) host memory, suitable for DMA transfers
 * - HOST_PAGEABLE: Regular pageable host memory, requires staging for GPU transfers
 * - CUDA: Device (GPU) memory
 *
 * Note: HOST is an alias for HOST_PINNED for backward compatibility.
 */
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

/**
 * @brief Check if a location is on the host (either pinned or pageable).
 */
inline bool isHostLocation(DataLocation location)
{
    return location == DataLocation::HOST_PINNED || location == DataLocation::HOST_PAGEABLE;
}

/**
 * @brief Check if a location is on the CUDA device (owned or view).
 */
inline bool isCudaLocation(DataLocation location)
{
    return location == DataLocation::CUDA || location == DataLocation::CUDA_VIEW;
}

/**
 * @brief Check if a location is a non-owning view.
 */
inline bool isViewLocation(DataLocation location)
{
    return location == DataLocation::VIEW || location == DataLocation::CUDA_VIEW;
}

} // namespace velodb
