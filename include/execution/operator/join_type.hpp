#pragma once

namespace velodb {

// Join type enumeration
enum class JoinType {
    INNER,
    LEFT,
    RIGHT,
    FULL,
    CROSS
};

} // namespace velodb
