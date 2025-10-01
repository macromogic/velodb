#include "data/bit_vector.hpp"

#include "common/exception.hpp"
#include "cuda/event.hpp"
#include "cuda/helper.hpp"
#include "cuda/stream.hpp"
#include "data/data_location.hpp"

#include <cuda_runtime.h>

namespace velodb {

BitVector::BitVector(size_t num_bits)
    : size_(num_bits)
    , location_(DataLocation::HOST)
{
    data_ = new Element[(num_bits + ELEMENT_WIDTH - 1) / ELEMENT_WIDTH];
}

BitVector::BitVector(const BitVector& other)
    : size_(other.size_)
    , location_(other.location_)
{
    auto num_elements = (size_ + ELEMENT_WIDTH - 1) / ELEMENT_WIDTH;
    switch (location_) {
    case DataLocation::HOST:
        data_ = new Element[num_elements];
        std::copy(other.data_, other.data_ + num_elements, data_);
        break;
    case DataLocation::CUDA:
        CHECKED_CALL_THROW(cudaMalloc(&data_, num_elements * sizeof(Element)));
        CHECKED_CALL_THROW(cudaMemcpy(data_, other.data_, num_elements, cudaMemcpyDeviceToDevice));
        break;
    case DataLocation::VIEW:
        data_ = other.data_;
        break;
    default:
        __builtin_unreachable();
    }
}

BitVector::BitVector(BitVector&& other) noexcept
    : size_(other.size_)
    , location_(other.location_)
    , data_(nullptr)
{
    std::swap(data_, other.data_);
}

BitVector& BitVector::operator=(const BitVector& other)
{
    size_ = other.size_;
    location_ = other.location_;
    auto num_elements = (size_ + ELEMENT_WIDTH - 1) / ELEMENT_WIDTH;
    switch (location_) {
    case DataLocation::HOST:
        data_ = new Element[num_elements];
        std::copy(other.data_, other.data_ + num_elements, data_);
        break;
    case DataLocation::CUDA:
        CHECKED_CALL_THROW(cudaMalloc(&data_, num_elements * sizeof(Element)));
        CHECKED_CALL_THROW(cudaMemcpy(data_, other.data_, num_elements, cudaMemcpyDeviceToDevice));
        break;
    case DataLocation::VIEW:
        data_ = other.data_;
        break;
    default:
        __builtin_unreachable();
    }
    return *this;
}

BitVector& BitVector::operator=(BitVector&& other) noexcept
{
    size_ = other.size_;
    location_ = other.location_;
    data_ = other.data_;
    other.data_ = nullptr;
    return *this;
}

BitVector::~BitVector()
{
    switch (location_) {
    case DataLocation::HOST:
        delete[] data_;
        break;
    case DataLocation::CUDA:
        cudaFree(data_);
        break;
    default:
        break;
    }
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
    auto* new_data = new Element[new_element_count];
    std::move(data_, data_ + size_, new_data);
    std::swap(data_, new_data);
    delete[] new_data;
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

    const Element* src_data = data_ + start_index;
    Element* dest_data = result.data_;
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

void BitVector::append(const BitVector& other)
{
    size_t original_size = size_;
    resize(size_ + other.size_);
    size_t start_offset = original_size % ELEMENT_WIDTH;
    size_t start_index = original_size / ELEMENT_WIDTH;
    size_t n_full_elements = other.size_ / ELEMENT_WIDTH;
    size_t remaining_bits = other.size_ % ELEMENT_WIDTH;

    const Element* src_data = other.data_;
    Element* dest_data = data_ + start_index;
    // Copy full elements
    for (size_t i = 0; i < n_full_elements; ++i) {
        dest_data[i] |= (src_data[i] << start_offset);
        dest_data[i + 1] = (src_data[i] >> (ELEMENT_WIDTH - start_offset));
    }

    // Copy the remaining bits in the last partial element
    if (remaining_bits > 0) {
        dest_data[n_full_elements] |= (src_data[n_full_elements] << start_offset);
        if (start_offset + remaining_bits > ELEMENT_WIDTH) {
            dest_data[n_full_elements + 1] = (src_data[n_full_elements] >> (ELEMENT_WIDTH - start_offset));
        }
    }
}

void BitVector::to(DataLocation location)
{
    VELODB_ASSERT_MSG(location != DataLocation::VIEW, "Cannot move data to VIEW");
    if (location_ != location) {
        if (location_ == DataLocation::CUDA) {
            Element* host_data = new Element[size_];
            auto& stream = CudaStream::getD2HStream();
            CHECKED_CALL_THROW(
                cudaMemcpyAsync(host_data, data_, size_ * sizeof(Element), cudaMemcpyDeviceToHost, stream.get()));
            stream.synchronize();
            cudaFree(data_);
            data_ = host_data;
        } else {
            Element* device_data;
            auto& stream = CudaStream::getH2DStream();
            CHECKED_CALL_THROW(cudaMalloc(&device_data, size_ * sizeof(Element)));
            CHECKED_CALL_THROW(
                cudaMemcpyAsync(device_data, data_, size_ * sizeof(Element), cudaMemcpyHostToDevice, stream.get()));
            stream.synchronize();
            if (location_ == DataLocation::HOST) {
                delete[] data_;
            }
            data_ = device_data;
        }
        location_ = location;
    }
}

} // namespace velodb
