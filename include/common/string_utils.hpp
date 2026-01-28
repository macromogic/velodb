#pragma once

#include <string>
#include <utility>

namespace velodb {

inline std::pair<std::string, std::string> splitName(const std::string& name)
{
    auto pos = name.find('.');
    if (pos == std::string::npos) {
        return { "", name };
    }
    return { name.substr(0, pos), name.substr(pos + 1) };
}

} // namespace velodb
