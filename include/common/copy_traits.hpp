#pragma once

#include <memory>

namespace velodb {

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

template <typename T>
class Cloneable {
public:
    T clone() const
    {
        return static_cast<const T*>(this)->cloneImpl();
    }
};

template <typename T>
class UniqueCloneable {
public:
    std::unique_ptr<T> cloneUnique() const
    {
        return static_cast<const T*>(this)->cloneUniqueImpl();
    }
};

} // namespace velodb
