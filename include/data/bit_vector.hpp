#pragma once

#include "cuda/event_pool.hpp"
#include "data/data_location.hpp"

#include <cstddef>
#include <cstdint>

namespace velodb {

class BitVector {
public:
    using Element = uint64_t;
    static constexpr size_t ELEMENT_WIDTH = 8 * sizeof(Element);

    BitVector(size_t num_bits);
    BitVector(const BitVector&);
    BitVector(BitVector&&) noexcept;
    BitVector& operator=(const BitVector&);
    BitVector& operator=(BitVector&&) noexcept;
    ~BitVector();

    void set(size_t index);
    void unset(size_t index);
    bool get(size_t index) const;
    void resize(size_t new_size);
    BitVector slice(size_t start, size_t end) const;
    void append(const BitVector& other);
    void to(DataLocation location);

private:
    size_t size_;
    DataLocation location_;
    Element* data_;
};

} // namespace velodb
