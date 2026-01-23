#pragma once

#include "common/exception.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>

namespace velodb {

template <typename T, typename E>
class Result;

// Helper trait to detect Result types
template <typename T>
struct IsResult : std::false_type { };

template <typename T, typename E>
struct IsResult<Result<T, E>> : std::true_type { };

template <typename T>
inline constexpr bool IsResultV = IsResult<T>::value;

template <typename T, typename E = std::string>
class Result {
private:
    // Type aliases for cleaner andThen signatures
    template <typename F, typename Arg>
    using InvokeResult = std::invoke_result_t<F, Arg>;

    template <typename F>
    using InvokeResultVoid = std::invoke_result_t<F>;

    template <typename F, typename Arg>
    using EnableResultReturning = std::enable_if_t<IsResultV<InvokeResult<F, Arg>>, InvokeResult<F, Arg>>;

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

    // Copy constructor
    Result(const Result&) = default;

    // Move constructor
    Result(Result&&) = default;

    // Copy assignment operator
    Result& operator=(const Result&) = default;

    // Move assignment operator
    Result& operator=(Result&&) = default;

    // Destructor
    ~Result() = default;

    std::optional<T> ok() const noexcept
    {
        if (std::holds_alternative<T>(value_)) {
            return std::get<T>(value_);
        }
        return std::nullopt;
    }

    explicit operator bool() const noexcept { return std::holds_alternative<T>(value_); }

    // Access methods
    T& value() &
    {
        if (!*this) {
            VELODB_THROW(DatabaseError, "Attempted to access value of Result containing error");
        }
        return std::get<T>(value_);
    }

    const T& value() const&
    {
        if (!*this) {
            VELODB_THROW(DatabaseError, "Attempted to access value of Result containing error");
        }
        return std::get<T>(value_);
    }

    T&& value() &&
    {
        if (!*this) {
            VELODB_THROW(DatabaseError, "Attempted to access value of Result containing error");
        }
        return std::move(std::get<T>(value_));
    }

    const E& error() const&
    {
        if (*this) {
            VELODB_THROW(DatabaseError, "Attempted to access error of Result containing value");
        }
        return std::get<E>(value_);
    }

    E&& error() &&
    {
        if (*this) {
            VELODB_THROW(DatabaseError, "Attempted to access error of Result containing value");
        }
        return std::move(std::get<E>(value_));
    }

    // Convenience methods
    T value_or(const T& default_value) const& { return *this ? std::get<T>(value_) : default_value; }

    T value_or(T&& default_value) && { return *this ? std::move(std::get<T>(value_)) : std::move(default_value); }

    template <typename ExceptionType = DatabaseError, typename... Args>
    const T& value_or_throw(Args&&... args) const&
    {
        if (*this) {
            return std::get<T>(value_);
        } else {
            throw ExceptionType(std::forward<Args>(args)...);
        }
    }

    template <typename ExceptionType = DatabaseError, typename... Args>
    T value_or_throw(Args&&... args) &&
    {
        if (*this) {
            return std::move(std::get<T>(value_));
        } else {
            throw ExceptionType(std::forward<Args>(args)...);
        }
    }

    // Monadic operations
    template <typename F>
    auto andThen(F&& f) & -> EnableResultReturning<F, T&>
    {
        static_assert(std::is_invocable_v<F, T&>, "Function must be invocable with T&");
        static_assert(IsResultV<InvokeResult<F, T&>>, "Function must return a Result type");
        if (*this) {
            return std::invoke(std::forward<F>(f), value());
        } else {
            using ReturnType = InvokeResult<F, T&>;
            return ReturnType::failure(error());
        }
    }

    template <typename F>
    auto andThen(F&& f) const& -> EnableResultReturning<F, const T&>
    {
        static_assert(std::is_invocable_v<F, const T&>, "Function must be invocable with const T&");
        static_assert(IsResultV<InvokeResult<F, const T&>>, "Function must return a Result type");
        if (*this) {
            return std::invoke(std::forward<F>(f), value());
        } else {
            using ReturnType = InvokeResult<F, const T&>;
            return ReturnType::failure(error());
        }
    }

    template <typename F>
    auto andThen(F&& f) && -> EnableResultReturning<F, T&&>
    {
        static_assert(std::is_invocable_v<F, T&&>, "Function must be invocable with T&&");
        static_assert(IsResultV<InvokeResult<F, T&&>>, "Function must return a Result type");
        if (*this) {
            return std::invoke(std::forward<F>(f), std::move(value()));
        } else {
            using ReturnType = InvokeResult<F, T&&>;
            return ReturnType::failure(std::move(error()));
        }
    }

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
private:
    // Type aliases for cleaner andThen signatures
    template <typename F>
    using InvokeResult = std::invoke_result_t<F>;

    template <typename F>
    using EnableResultReturning = std::enable_if_t<IsResultV<InvokeResult<F>>, InvokeResult<F>>;

public:
    explicit Result(E&& error)
        : error_(std::forward<E>(error))
    {
    }
    explicit Result(const E& error)
        : error_(error)
    {
    }

    // Copy constructor
    Result(const Result&) = default;

    // Move constructor
    Result(Result&&) = default;

    // Copy assignment operator
    Result& operator=(const Result&) = default;

    // Move assignment operator
    Result& operator=(Result&&) = default;

    // Destructor
    ~Result() = default;

    explicit operator bool() const noexcept { return !error_.has_value(); }

    // Access methods
    const E& error() const&
    {
        if (*this) {
            VELODB_THROW(DatabaseError, "Attempted to access error of Result containing value");
        }
        return *error_;
    }

    E&& error() &&
    {
        if (*this) {
            VELODB_THROW(DatabaseError, "Attempted to access error of Result containing value");
        }
        return std::move(*error_);
    }

    // Monadic operations
    template <typename F>
    auto andThen(F&& f) & -> EnableResultReturning<F>
    {
        static_assert(std::is_invocable_v<F>, "Function must be invocable with no arguments");
        static_assert(IsResultV<InvokeResult<F>>, "Function must return a Result type");
        if (*this) {
            return std::invoke(std::forward<F>(f));
        } else {
            using ReturnType = InvokeResult<F>;
            return ReturnType::failure(error());
        }
    }

    template <typename F>
    auto andThen(F&& f) const& -> EnableResultReturning<F>
    {
        static_assert(std::is_invocable_v<F>, "Function must be invocable with no arguments");
        static_assert(IsResultV<InvokeResult<F>>, "Function must return a Result type");
        if (*this) {
            return std::invoke(std::forward<F>(f));
        } else {
            using ReturnType = InvokeResult<F>;
            return ReturnType::failure(error());
        }
    }

    template <typename F>
    auto andThen(F&& f) && -> EnableResultReturning<F>
    {
        static_assert(std::is_invocable_v<F>, "Function must be invocable with no arguments");
        static_assert(IsResultV<InvokeResult<F>>, "Function must return a Result type");
        if (*this) {
            return std::invoke(std::forward<F>(f));
        } else {
            using ReturnType = InvokeResult<F>;
            return ReturnType::failure(std::move(error()));
        }
    }

    // Factory methods
    static Result success() { return Result(); }

    static Result failure(E&& error) { return Result(std::forward<E>(error)); }

    static Result failure(const E& error) { return Result(error); }

private:
    Result() = default;

    std::optional<E> error_;
};

} // namespace velodb
