#pragma once

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>

namespace velodb {

// Fixed-length string type that's CUDA-compatible
// Uses a runtime-determined maximum length for flexible VARCHAR support
class FixedString {
public:
    static constexpr size_t DEFAULT_MAX_LENGTH = 256;

    // Default constructor - creates empty string with default max length
    FixedString()
        : max_length_(DEFAULT_MAX_LENGTH)
        , data_(new char[max_length_])
    {
        data_[0] = '\0';
    }

    // Constructor with specified max length
    explicit FixedString(size_t max_length)
        : max_length_(std::max(max_length, size_t(1))) // Ensure at least 1 byte for null terminator
        , data_(new char[max_length_]) // FIXED: Use max_length_ not max_length
    {
        data_[0] = '\0';
    }

    // Constructor from std::string with default max length
    explicit FixedString(const std::string& str)
        : max_length_(std::max(str.length() + 1, DEFAULT_MAX_LENGTH))
        , data_(new char[max_length_])
    {
        const size_t len = std::min(str.length(), max_length_ - 1);
        std::memcpy(data_, str.c_str(), len);
        data_[len] = '\0';
    }

    // Constructor from std::string with specified max length
    FixedString(const std::string& str, size_t max_length)
        : max_length_(std::max(max_length, size_t(1)))
        , data_(new char[max_length_])
    {
        const size_t len = std::min(str.length(), max_length_ - 1);
        std::memcpy(data_, str.c_str(), len);
        data_[len] = '\0';
    }

    // Constructor from C string with specified max length
    FixedString(const char* str, size_t max_length)
        : max_length_(std::max(max_length, size_t(1)))
        , data_(new char[max_length_])
    {
        if (str) {
            const size_t len = std::min(std::strlen(str), max_length_ - 1);
            std::memcpy(data_, str, len);
            data_[len] = '\0';
        } else {
            data_[0] = '\0';
        }
    }

    // Copy constructor
    FixedString(const FixedString& other)
        : max_length_(other.max_length_)
        , data_(new char[max_length_])
    {
        std::memcpy(data_, other.data_, max_length_);
    }

    // Move constructor
    FixedString(FixedString&& other) noexcept
        : max_length_(other.max_length_)
        , data_(other.data_)
    {
        other.data_ = nullptr;
        other.max_length_ = 0;
    }

    // Destructor
    ~FixedString() { delete[] data_; }

    // Copy assignment operator
    FixedString& operator=(const FixedString& other)
    {
        if (this != &other) {
            if (max_length_ != other.max_length_) {
                delete[] data_;
                max_length_ = other.max_length_;
                data_ = new char[max_length_];
            }
            std::memcpy(data_, other.data_, max_length_);
        }
        return *this;
    }

    // Move assignment operator
    FixedString& operator=(FixedString&& other) noexcept
    {
        if (this != &other) {
            delete[] data_;
            max_length_ = other.max_length_;
            data_ = other.data_;
            other.data_ = nullptr;
            other.max_length_ = 0;
        }
        return *this;
    }

    // Assignment from std::string
    FixedString& operator=(const std::string& str)
    {
        const size_t len = std::min(str.length(), max_length_ - 1);
        std::memcpy(data_, str.c_str(), len);
        data_[len] = '\0';
        return *this;
    }

    // Convert to std::string
    std::string toString() const
    {
        if (!data_) {
            return "(null)";
        }
        return std::string(data_);
    }

    // Get C string
    const char* c_str() const { return data_; }

    // Get data pointer (for CUDA operations)
    char* data() { return data_; }

    const char* data() const { return data_; }

    // Get maximum length
    size_t max_length() const { return max_length_; }

    // Get current length
    size_t length() const { return std::strlen(data_); }

    // Check if empty
    bool empty() const { return data_[0] == '\0'; }

    // Comparison operators
    bool operator==(const FixedString& other) const { return std::strcmp(data_, other.data_) == 0; }

    bool operator!=(const FixedString& other) const { return !(*this == other); }

    bool operator<(const FixedString& other) const { return std::strcmp(data_, other.data_) < 0; }

private:
    size_t max_length_;
    char* data_;
};

} // namespace velodb
