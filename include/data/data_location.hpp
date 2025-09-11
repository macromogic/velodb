#pragma once

namespace velodb {

enum class DataLocation {
    VIEW,
    HOST,
    CUDA,
};

inline auto format_as(DataLocation location)
{
    switch (location) {
    case DataLocation::HOST:
        return "HOST";
    case DataLocation::CUDA:
        return "CUDA";
    case DataLocation::VIEW:
        return "VIEW";
    default:
        return "UNKNOWN";
    }
}

} // namespace velodb
