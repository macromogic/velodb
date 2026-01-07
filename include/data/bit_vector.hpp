#pragma once

#include "data/data_location.hpp"

#include <cstddef>
#include <cstdint>

namespace velodb {

template <typename T>
class ValueVector;
template <typename DT, template <typename> class VecT>
class ValueVectorBase;

class BitVector {
public:
    using Element = uint64_t;
    static constexpr size_t ELEMENT_WIDTH = 8 * sizeof(Element);

    explicit BitVector(size_t num_bits);
    BitVector(const BitVector&);
    BitVector(BitVector&&) noexcept;
    BitVector& operator=(const BitVector&);
    BitVector& operator=(BitVector&&) noexcept;
    ~BitVector();

    void set(size_t index);
    void unset(size_t index);
    bool get(size_t index) const;
    void resize(size_t new_size);
    void reserve(size_t new_capacity);
    BitVector slice(size_t start, size_t end) const;
    BitVector splitFront(size_t size);
    void append(const BitVector& other);
    void to(DataLocation location);

private:
    size_t size_;
    size_t element_capacity_;
    DataLocation location_;
    Element* data_;

    template <typename T>
    friend class ValueVector;
    template <typename DT, template <typename> class VecT>
    friend class ValueVectorBase;
};

} // namespace velodb
