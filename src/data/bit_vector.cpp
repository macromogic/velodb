#include "data/bit_vector.hpp"

#include "common/exception.hpp"

namespace velodb {

BitVector::BitVector(size_t num_bits)
    : data_((num_bits + ELEMENT_WIDTH - 1) / ELEMENT_WIDTH, 0)
    , size_(num_bits)
{
}

void BitVector::set(size_t index)
{
    VELODB_ASSERT_MSG(index < size_, "Invalid index");
    size_t element_index = index / ELEMENT_WIDTH;
    size_t bit_index = index % ELEMENT_WIDTH;
    data_[element_index] |= (Element(1) << bit_index);
}

void BitVector::unset(size_t index)
{
    VELODB_ASSERT_MSG(index < size_, "Invalid index");
    size_t element_index = index / ELEMENT_WIDTH;
    size_t bit_index = index % ELEMENT_WIDTH;
    data_[element_index] &= ~(Element(1) << bit_index);
}

bool BitVector::get(size_t index) const
{
    VELODB_ASSERT_MSG(index < size_, "Invalid index");
    size_t element_index = index / ELEMENT_WIDTH;
    size_t bit_index = index % ELEMENT_WIDTH;
    return (data_[element_index] >> bit_index) & Element(1);
}

void BitVector::resize(size_t new_size)
{
    size_t new_element_count = (new_size + ELEMENT_WIDTH - 1) / ELEMENT_WIDTH;
    data_.resize(new_element_count);
    size_ = new_size;
}

BitVector BitVector::slice(size_t start, size_t end) const
{
    VELODB_ASSERT_MSG(start <= end && end <= size_, "Invalid slice range");

    size_t slice_bits = end - start;
    BitVector result(slice_bits);

    size_t start_offset = start % ELEMENT_WIDTH;
    size_t start_index = start / ELEMENT_WIDTH;
    size_t n_elements = (slice_bits + ELEMENT_WIDTH - 1) / ELEMENT_WIDTH;
    size_t last_effective_bits = slice_bits % ELEMENT_WIDTH;

    const Element* src_data = data_.data() + start_index;
    Element* dest_data = result.data_.data();
    // Copy full elements
    for (size_t i = 0; i < n_elements; ++i) {
        if (i > 0 && start_offset > 0) {
            dest_data[i - 1] |= (src_data[i] << (ELEMENT_WIDTH - start_offset));
        }
        dest_data[i] = (src_data[i] >> start_offset);
    }

    // Mask out bits beyond the slice in the last element
    if (last_effective_bits > 0) {
        dest_data[n_elements - 1] &= (Element(1) << last_effective_bits) - 1;
    }

    return result;
}

} // namespace velodb
