#pragma once

namespace velodb {

/**
 * @brief Base class for types that should not be copyable
 * 
 * This class provides a consistent way to make classes non-copyable
 * while still allowing move semantics. Classes that manage unique
 * resources should inherit from this class privately.
 */
class NonCopyable {
protected:
    NonCopyable() = default;
    virtual ~NonCopyable() = default;
    
    // Allow move construction and assignment
    NonCopyable(NonCopyable&&) = default;
    NonCopyable& operator=(NonCopyable&&) = default;
    
private:
    // Prevent copying
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
};

} // namespace velodb
