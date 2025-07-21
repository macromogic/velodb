#pragma once

#include "result.hpp"
#include "traced_exception.hpp"
#include <memory>
#include <optional>
#include <functional>

namespace velodb {

/**
 * @brief Factory function utilities for modern C++ ownership patterns
 */
namespace factory {

/**
 * @brief Create a unique_ptr using perfect forwarding
 * 
 * @tparam T Type to create
 * @tparam Args Constructor argument types
 * @param args Constructor arguments
 * @return std::unique_ptr<T> Unique pointer to the created object
 */
template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

/**
 * @brief Create a shared_ptr using perfect forwarding
 * 
 * @tparam T Type to create
 * @tparam Args Constructor argument types
 * @param args Constructor arguments
 * @return std::shared_ptr<T> Shared pointer to the created object
 */
template<typename T, typename... Args>
std::shared_ptr<T> make_shared(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

/**
 * @brief Safe factory function that returns optional on failure
 * 
 * @tparam T Type to create
 * @tparam Args Constructor argument types
 * @param args Constructor arguments
 * @return std::optional<std::unique_ptr<T>> Optional unique pointer
 */
template<typename T, typename... Args>
std::optional<std::unique_ptr<T>> try_make_unique(Args&&... args) noexcept {
    try {
        return std::make_unique<T>(std::forward<Args>(args)...);
    } catch (...) {
        return std::nullopt;
    }
}

/**
 * @brief Safe factory function that returns Result on failure
 * 
 * @tparam T Type to create
 * @tparam Args Constructor argument types
 * @param args Constructor arguments
 * @return UniqueResult<T> Result containing unique pointer or error
 */
template<typename T, typename... Args>
UniqueResult<T> make_unique_result(Args&&... args) noexcept {
    try {
        return UniqueResult<T>::success(std::make_unique<T>(std::forward<Args>(args)...));
    } catch (const TracedException& e) {
        return UniqueResult<T>::failure(e.what());
    } catch (...) {
        return UniqueResult<T>::failure("Unknown error during object construction");
    }
}

/**
 * @brief Safe factory function that returns Result on failure
 * 
 * @tparam T Type to create
 * @tparam Args Constructor argument types
 * @param args Constructor arguments
 * @return SharedResult<T> Result containing shared pointer or error
 */
template<typename T, typename... Args>
SharedResult<T> make_shared_result(Args&&... args) noexcept {
    try {
        return SharedResult<T>::success(std::make_shared<T>(std::forward<Args>(args)...));
    } catch (const TracedException& e) {
        return SharedResult<T>::failure(e.what());
    } catch (...) {
        return SharedResult<T>::failure("Unknown error during object construction");
    }
}

/**
 * @brief Factory function with custom deleter
 * 
 * @tparam T Type to create
 * @tparam Deleter Custom deleter type
 * @tparam Args Constructor argument types
 * @param deleter Custom deleter
 * @param args Constructor arguments
 * @return std::unique_ptr<T, Deleter> Unique pointer with custom deleter
 */
template<typename T, typename Deleter, typename... Args>
std::unique_ptr<T, Deleter> make_unique_with_deleter(Deleter deleter, Args&&... args) {
    return std::unique_ptr<T, Deleter>(new T(std::forward<Args>(args)...), deleter);
}

} // namespace factory

} // namespace velodb
