#pragma once

#include <fmt/format.h>

#include <memory>
#include <string>

namespace velodb {

template <typename T>
auto format_as(const T& value) -> std::string
{
    return value.toString();
}

template <typename T>
auto format_as(const std::unique_ptr<T>& ptr) -> std::string
{
    if (ptr) {
        return ptr->toString();
    } else {
        return "null";
    }
}

template <typename T>
auto format_as(const std::shared_ptr<T>& ptr) -> std::string
{
    if (ptr) {
        return ptr->toString();
    } else {
        return "null";
    }
}

} // namespace velodb
