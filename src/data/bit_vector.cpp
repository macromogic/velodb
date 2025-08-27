#include "data/bit_vector.hpp"

namespace velodb {

BitVector::BitVector(size_t num_bits)
    : data_((num_bits + ELEMENT_WIDTH - 1) / ELEMENT_WIDTH, 0)
{
}

void BitVector::set(size_t index)
{
    size_t element_index = index / ELEMENT_WIDTH;
    size_t bit_index = index % ELEMENT_WIDTH;
    data_[element_index] |= (Element(1) << bit_index);
}

void BitVector::unset(size_t index)
{
    size_t element_index = index / ELEMENT_WIDTH;
    size_t bit_index = index % ELEMENT_WIDTH;
    data_[element_index] &= ~(Element(1) << bit_index);
}

bool BitVector::get(size_t index) const
{
    size_t element_index = index / ELEMENT_WIDTH;
    if (element_index >= data_.size()) {
        return false; // Out of bounds, assume false
    }
    size_t bit_index = index % ELEMENT_WIDTH;
    return (data_[element_index] >> bit_index) & Element(1);
}

void BitVector::resize(size_t new_size)
{
    size_t new_element_count = (new_size + ELEMENT_WIDTH - 1) / ELEMENT_WIDTH;
    data_.resize(new_element_count);
}

void BitVector::clear()
{
    data_.clear();
}

} // namespace velodb
