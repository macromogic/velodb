#pragma once

#include "common/exception.hpp"

#include <memory>
#include <optional>
#include <string>
#include <variant>

namespace velodb {

template <typename T, typename E = std::string>
class Result {
public:
    // Success constructor
    explicit Result(T&& value)
        : value_(std::forward<T>(value))
    {
    }
    explicit Result(const T& value)
        : value_(value)
    {
    }

    // Error constructor
    explicit Result(E&& error)
        : value_(std::forward<E>(error))
    {
    }
    explicit Result(const E& error)
        : value_(error)
    {
    }

    // Query methods
    bool ok() const noexcept { return std::holds_alternative<T>(value_); }

    bool err() const noexcept { return std::holds_alternative<E>(value_); }

    explicit operator bool() const noexcept { return ok(); }

    // Access methods
    T& value() &
    {
        if (!ok()) {
            VELODB_THROW(DatabaseError, "Attempted to access value of Result containing error");
        }
        return std::get<T>(value_);
    }

    const T& value() const&
    {
        if (!ok()) {
            VELODB_THROW(DatabaseError, "Attempted to access value of Result containing error");
        }
        return std::get<T>(value_);
    }

    T&& value() &&
    {
        if (!ok()) {
            VELODB_THROW(DatabaseError, "Attempted to access value of Result containing error");
        }
        return std::move(std::get<T>(value_));
    }

    const E& error() const&
    {
        if (!err()) {
            VELODB_THROW(DatabaseError, "Attempted to access error of Result containing value");
        }
        return std::get<E>(value_);
    }

    E&& error() &&
    {
        if (!err()) {
            VELODB_THROW(DatabaseError, "Attempted to access error of Result containing value");
        }
        return std::move(std::get<E>(value_));
    }

    // Convenience methods
    T value_or(const T& default_value) const& { return ok() ? value() : default_value; }

    T value_or(T&& default_value) && { return ok() ? std::move(value()) : std::move(default_value); }

    // Factory methods
    static Result success(T&& value) { return Result(std::forward<T>(value)); }

    static Result success(const T& value) { return Result(value); }

    static Result failure(E&& error) { return Result(std::forward<E>(error)); }

    static Result failure(const E& error) { return Result(error); }

private:
    std::variant<T, E> value_;
};

template <typename E>
class Result<void, E> {
public:
    explicit Result(E&& error)
        : error_(std::forward<E>(error))
    {
    }
    explicit Result(const E& error)
        : error_(error)
    {
    }
    // Query methods
    bool ok() const noexcept { return !error_.has_value(); }

    bool err() const noexcept { return error_.has_value(); }

    explicit operator bool() const noexcept { return ok(); }

    // Access methods
    const E& error() const&
    {
        if (!err()) {
            VELODB_THROW(DatabaseError, "Attempted to access error of Result containing value");
        }
        return *error_;
    }

    E&& error() &&
    {
        if (!err()) {
            VELODB_THROW(DatabaseError, "Attempted to access error of Result containing value");
        }
        return std::move(*error_);
    }

    // Factory methods
    static Result success() { return Result(); }

    static Result failure(E&& error) { return Result(std::forward<E>(error)); }

    static Result failure(const E& error) { return Result(error); }

private:
    Result() = default;

    std::optional<E> error_;
};

// Convenience aliases
template <typename T>
using UniqueResult = Result<std::unique_ptr<T>, std::string>;

template <typename T>
using SharedResult = Result<std::shared_ptr<T>, std::string>;

template <typename T>
using OptionalResult = Result<std::optional<T>, std::string>;

} // namespace velodb
