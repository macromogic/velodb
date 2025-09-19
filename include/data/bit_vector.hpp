#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace velodb {

class BitVector {
public:
    using Element = uint64_t;
    static constexpr size_t ELEMENT_WIDTH = 8 * sizeof(Element);

    BitVector(size_t num_bits);
    BitVector(const BitVector&) = default;
    ~BitVector() = default;

    void set(size_t index);
    void unset(size_t index);
    bool get(size_t index) const;
    void resize(size_t new_size);
    BitVector slice(size_t start, size_t end) const;
    void append(const BitVector& other);

private:
    std::vector<Element> data_;
    size_t size_;
};

} // namespace velodb
