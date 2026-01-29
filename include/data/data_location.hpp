#pragma once

namespace velodb {

/**
 * @brief Specifies where data is stored.
 *
 * - VIEW: A non-owning reference to data stored elsewhere
 * - HOST_PINNED: Pinned (page-locked) host memory, suitable for DMA transfers
 * - HOST_PAGEABLE: Regular pageable host memory, requires staging for GPU transfers
 * - CUDA: Device (GPU) memory
 *
 * Note: HOST is an alias for HOST_PINNED for backward compatibility.
 */
enum class DataLocation {
    VIEW,
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

} // namespace velodb
