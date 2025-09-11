#pragma once

#include "common/exception.hpp"

#include <optional>
#include <string>
#include <variant>

namespace velodb {

// Forward declarations
template <typename T>
class ValueVector;

class OrdinalString {
public:
    explicit OrdinalString(std::string str)
        : ordinal_(std::nullopt)
        , str_(std::move(str))
    {
    }

    explicit OrdinalString(std::string_view str)
        : ordinal_(std::nullopt)
        , str_(str)
    {
    }

    OrdinalString(size_t ordinal, std::string str)
        : ordinal_(ordinal)
        , str_(std::move(str))
    {
    }

    OrdinalString(size_t ordinal, std::string_view str)
        : ordinal_(ordinal)
        , str_(str)
    {
    }

    OrdinalString(const OrdinalString&) = default;
    OrdinalString& operator=(const OrdinalString&) = default;
    OrdinalString(OrdinalString&&) = default;
    OrdinalString& operator=(OrdinalString&&) = default;
    ~OrdinalString() = default;

    size_t getOrdinal() const
    {
        VELODB_ASSERT_MSG(ordinal_.has_value(), "Ordinal not set");
        return *ordinal_;
    }

    std::string toString() const
    {
        return std::visit(
            [this](auto&& arg) {
                auto str = std::string(arg);
                if (ordinal_.has_value()) {
                    str += " (ord: " + std::to_string(*ordinal_) + ")";
                }
                return str;
            },
            str_);
    }

    operator std::string() const
    {
        return std::visit([](auto&& arg) { return std::string(arg); }, str_);
    }

    bool operator<(const OrdinalString& other) const { return ordinal_ < other.ordinal_; }

    bool operator==(const OrdinalString& other) const { return ordinal_ == other.ordinal_; }

    bool operator!=(const OrdinalString& other) const { return !(*this == other); }

    bool operator<=(const OrdinalString& other) const { return !(other < *this); }

    bool operator>(const OrdinalString& other) const { return other < *this; }

    bool operator>=(const OrdinalString& other) const { return !(*this < other); }

private:
    std::optional<size_t> ordinal_;
    std::variant<std::string, std::string_view> str_;

    friend class ValueVector<size_t>;
};

} // namespace velodb
