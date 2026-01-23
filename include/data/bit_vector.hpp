#pragma once

#include "common/copy_traits.hpp"
#include "data/data_location.hpp"

#include <cstddef>
#include <cstdint>

namespace velodb {

template <typename T>
class ValueVector;
template <typename DT, template <typename> class VecT>
class ValueVectorBase;

class BitVector : private NonCopyable, public Cloneable<BitVector> {
public:
    using Element = uint8_t;

    explicit BitVector(size_t num_bits, DataLocation location = DataLocation::HOST);
    // BitVector(const BitVector&);
    BitVector(BitVector&&) noexcept;
    // BitVector& operator=(const BitVector&);
    BitVector& operator=(BitVector&&) noexcept;
    ~BitVector();

    BitVector cloneImpl() const;

    void set(size_t index);
    void unset(size_t index);
    bool get(size_t index) const;
    void resize(size_t new_size);
    void reserve(size_t new_capacity);
    BitVector slice(size_t start, size_t end) const;
    void append(const BitVector& other);
    void to(DataLocation location);
    Element* data() { return data_; }
    const Element* data() const { return data_; }

private:
    static void copyBits(Element* dest, size_t dest_offset, const Element* src, size_t src_offset, size_t num_bits);

    size_t size_;
    size_t element_capacity_;
    DataLocation location_;
    Element* data_;

    template <typename T>
    friend class ValueVector;
    template <typename DT, template <typename> class VecT>
    friend class ValueVectorBase;
    friend class Column;
};

} // namespace velodb
