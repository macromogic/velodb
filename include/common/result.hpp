#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>

namespace velodb {

/**
 * @brief Simple Result type for error handling without exceptions
 * 
 * This provides a basic alternative to std::expected for C++17.
 * 
 * @tparam T Success value type
 * @tparam E Error type (defaults to std::string)
 */
template<typename T, typename E = std::string>
class Result {
public:
    // Success constructor
    explicit Result(T&& value) : value_(std::forward<T>(value)) {}
    explicit Result(const T& value) : value_(value) {}
    
    // Error constructor
    explicit Result(E&& error) : value_(std::forward<E>(error)) {}
    explicit Result(const E& error) : value_(error) {}
    
    // Query methods
    [[nodiscard]] bool has_value() const noexcept {
        return std::holds_alternative<T>(value_);
    }
    
    [[nodiscard]] bool has_error() const noexcept {
        return std::holds_alternative<E>(value_);
    }
    
    [[nodiscard]] explicit operator bool() const noexcept {
        return has_value();
    }
    
    // Access methods
    [[nodiscard]] T& value() & {
        if (!has_value()) {
            throw std::runtime_error("Attempted to access value of Result containing error");
        }
        return std::get<T>(value_);
    }
    
    [[nodiscard]] const T& value() const& {
        if (!has_value()) {
            throw std::runtime_error("Attempted to access value of Result containing error");
        }
        return std::get<T>(value_);
    }
    
    [[nodiscard]] T&& value() && {
        if (!has_value()) {
            throw std::runtime_error("Attempted to access value of Result containing error");
        }
        return std::move(std::get<T>(value_));
    }
    
    [[nodiscard]] const E& error() const& {
        if (!has_error()) {
            throw std::runtime_error("Attempted to access error of Result containing value");
        }
        return std::get<E>(value_);
    }
    
    [[nodiscard]] E&& error() && {
        if (!has_error()) {
            throw std::runtime_error("Attempted to access error of Result containing value");
        }
        return std::move(std::get<E>(value_));
    }
    
    // Convenience methods
    [[nodiscard]] T value_or(const T& default_value) const& {
        return has_value() ? value() : default_value;
    }
    
    [[nodiscard]] T value_or(T&& default_value) && {
        return has_value() ? std::move(value()) : std::move(default_value);
    }
    
    // Factory methods
    static Result success(T&& value) {
        return Result(std::forward<T>(value));
    }
    
    static Result success(const T& value) {
        return Result(value);
    }
    
    static Result failure(E&& error) {
        return Result(std::forward<E>(error));
    }
    
    static Result failure(const E& error) {
        return Result(error);
    }

private:
    std::variant<T, E> value_;
};

// Convenience aliases
template<typename T>
using StringResult = Result<T, std::string>;

template<typename T>
using UniqueResult = Result<std::unique_ptr<T>, std::string>;

template<typename T>
using SharedResult = Result<std::shared_ptr<T>, std::string>;

template<typename T>
using OptionalResult = Result<std::optional<T>, std::string>;

} // namespace velodb
