#include "data/bit_vector.hpp"

#include "common/exception.hpp"
#include "common/profiler.hpp"
#include "cuda/event.hpp"
#include "cuda/helper.hpp"
#include "cuda/stream.hpp"
#include "data/data_location.hpp"

#include <cuda_runtime.h>

namespace velodb {

BitVector::BitVector(size_t num_bits)
    : size_(num_bits)
    , element_capacity_(std::max(DIV_UP(nextPow2(num_bits), ELEMENT_WIDTH), 1ul))
    , location_(DataLocation::HOST)
    , data_(nullptr)
{
    CHECKED_CALL_THROW(cudaMallocHost(&data_, element_capacity_ * sizeof(Element)));
    std::fill_n(data_, element_capacity_, Element(0));
}

BitVector::BitVector(const BitVector& other)
    : size_(other.size_)
    , element_capacity_(other.element_capacity_)
    , location_(other.location_)
    , data_(nullptr)
{
    switch (location_) {
    case DataLocation::HOST:
        CHECKED_CALL_THROW(cudaMallocHost(&data_, element_capacity_ * sizeof(Element)));
        std::copy(other.data_, other.data_ + element_capacity_, data_);
        break;
    case DataLocation::CUDA:
        CHECKED_CALL_THROW(cudaMalloc(&data_, element_capacity_ * sizeof(Element)));
        CHECKED_CALL_THROW(cudaMemcpy(data_, other.data_, element_capacity_, cudaMemcpyDeviceToDevice));
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
    , element_capacity_(other.element_capacity_)
    , location_(other.location_)
    , data_(nullptr)
{
    std::swap(data_, other.data_);
}

BitVector& BitVector::operator=(const BitVector& other)
{
    size_ = other.size_;
    location_ = other.location_;
    element_capacity_ = other.element_capacity_;
    switch (location_) {
    case DataLocation::HOST:
        CHECKED_CALL_THROW(cudaMallocHost(&data_, element_capacity_ * sizeof(Element)));
        std::copy(other.data_, other.data_ + element_capacity_, data_);
        break;
    case DataLocation::CUDA:
        CHECKED_CALL_THROW(cudaMalloc(&data_, element_capacity_ * sizeof(Element)));
        CHECKED_CALL_THROW(cudaMemcpy(data_, other.data_, element_capacity_, cudaMemcpyDeviceToDevice));
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
    element_capacity_ = other.element_capacity_;
    data_ = other.data_;
    other.data_ = nullptr;
    return *this;
}

BitVector::~BitVector()
{
    switch (location_) {
    case DataLocation::HOST:
        cudaFreeHost(data_);
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
    VELODB_ASSERT_MSG(location_ != DataLocation::CUDA, "Cannot access data on device");
    VELODB_ASSERT_MSG(index < size_, "Invalid index");
    size_t element_index = index / ELEMENT_WIDTH;
    size_t bit_index = index % ELEMENT_WIDTH;
    data_[element_index] |= (Element(1) << bit_index);
}

void BitVector::unset(size_t index)
{
    VELODB_ASSERT_MSG(location_ != DataLocation::CUDA, "Cannot access data on device");
    VELODB_ASSERT_MSG(index < size_, "Invalid index");
    size_t element_index = index / ELEMENT_WIDTH;
    size_t bit_index = index % ELEMENT_WIDTH;
    data_[element_index] &= ~(Element(1) << bit_index);
}

bool BitVector::get(size_t index) const
{
    VELODB_ASSERT_MSG(location_ != DataLocation::CUDA, "Cannot access data on device");
    VELODB_ASSERT_MSG(index < size_, "Invalid index");
    size_t element_index = index / ELEMENT_WIDTH;
    size_t bit_index = index % ELEMENT_WIDTH;
    return (data_[element_index] >> bit_index) & Element(1);
}

void BitVector::resize(size_t new_size)
{
    VELODB_ASSERT_MSG(location_ == DataLocation::HOST, "Cannot resize non-host BitVector");
    reserve(new_size);
    size_t old_element_count = DIV_UP(size_, ELEMENT_WIDTH);
    size_t new_element_count = DIV_UP(new_size, ELEMENT_WIDTH);
    if (new_element_count < old_element_count) {
        // Clear bits in the last element if size is reduced
        std::fill_n(data_ + new_element_count, old_element_count - new_element_count, Element(0));
        size_t last_effective_bits = new_size % ELEMENT_WIDTH;
        if (last_effective_bits > 0) {
            data_[new_element_count - 1] &= (Element(1) << last_effective_bits) - 1;
        }
    }
    size_ = new_size;
}

void BitVector::reserve(size_t new_capacity)
{
    VELODB_ASSERT_MSG(location_ != DataLocation::VIEW, "Cannot reserve a VIEW BitVector");
    size_t new_element_capacity = DIV_UP(nextPow2(new_capacity), ELEMENT_WIDTH);
    if (new_element_capacity > element_capacity_) {
        Element* new_data;
        if (location_ == DataLocation::CUDA) {
            CHECKED_CALL_THROW(cudaMalloc(&new_data, new_element_capacity * sizeof(Element)));
            CHECKED_CALL_THROW(
                cudaMemcpy(new_data, data_, element_capacity_ * sizeof(Element), cudaMemcpyDeviceToDevice));
            CHECKED_CALL_THROW(cudaMemset(new_data + element_capacity_,
                                          0,
                                          (new_element_capacity - element_capacity_) * sizeof(Element)));
            cudaFree(data_);
        } else {
            CHECKED_CALL_THROW(cudaMallocHost(&new_data, new_element_capacity * sizeof(Element)));
            std::move(data_, data_ + element_capacity_, new_data);
            std::fill_n(new_data + element_capacity_, new_element_capacity - element_capacity_, Element(0));
            cudaFreeHost(data_);
        }
        data_ = new_data;
        element_capacity_ = new_element_capacity;
    }
}

BitVector BitVector::slice(size_t start, size_t end) const
{
    VELODB_ASSERT_MSG(start <= end && end <= size_, "Invalid slice range");
    VELODB_ASSERT_MSG(location_ == DataLocation::HOST, "Cannot slice non-host data");

    size_t slice_bits = end - start;
    BitVector result(slice_bits);

    if (slice_bits > 0) {
        copyBits(result.data_, 0, data_, start, slice_bits);
    }

    return result;
}

BitVector BitVector::splitFront(size_t size)
{
    VELODB_ASSERT_MSG(size <= size_, "Split size exceeds BitVector size");
    VELODB_ASSERT_MSG(location_ == DataLocation::HOST, "Cannot split non-host BitVector");

    BitVector split_part = slice(0, size);
    size_t remaining_size = size_ - size;

    // Shift the remaining bits to the front
    if (remaining_size > 0) {
        copyBits(data_, 0, data_, size, remaining_size);
    }

    // Clear out the now-unused elements at the end
    // Note: copyBits doesn't zero out bits beyond the copied range, so we must clean up manually.
    // Calculate how many full elements are now valid
    size_t valid_full_elements = remaining_size / ELEMENT_WIDTH;
    size_t valid_bits_in_last = remaining_size % ELEMENT_WIDTH;

    // 1. Mask the last valid element if it's partially used
    if (valid_bits_in_last > 0) {
        data_[valid_full_elements] &= (Element(1) << valid_bits_in_last) - 1;
    }

    // 2. Zero out rest of the elements
    size_t elements_used = DIV_UP(remaining_size, ELEMENT_WIDTH);
    if (elements_used < element_capacity_) {
        std::fill_n(data_ + elements_used, element_capacity_ - elements_used, Element(0));
    }

    size_ = remaining_size;
    return split_part;
}

void BitVector::append(const BitVector& other)
{
    size_t original_size = size_;
    resize(size_ + other.size_);

    copyBits(data_, original_size, other.data_, 0, other.size_);
}

void BitVector::copyBits(Element* dest, size_t dest_offset, const Element* src, size_t src_offset, size_t num_bits)
{
    if (num_bits == 0)
        return;

    size_t dest_idx = dest_offset / ELEMENT_WIDTH;
    size_t dest_shift = dest_offset % ELEMENT_WIDTH;

    size_t src_idx = src_offset / ELEMENT_WIDTH;
    size_t src_shift = src_offset % ELEMENT_WIDTH;

    size_t bits_remaining = num_bits;

    while (bits_remaining > 0) {
        // How many bits can we write to the current dest element?
        size_t bits_to_write = std::min(bits_remaining, ELEMENT_WIDTH - dest_shift);

        // Read up to bits_to_write bits from src
        // Shift down so desired bits are at LSB
        Element src_val = src[src_idx] >> src_shift;

        // If the needed bits span across two src elements, get the rest from the next element
        if (ELEMENT_WIDTH - src_shift < bits_to_write) {
            src_val |= (src[src_idx + 1] << (ELEMENT_WIDTH - src_shift));
        }

        // Mask the bits we are interested in to avoid garbage
        Element mask;
        if (bits_to_write < ELEMENT_WIDTH) {
            mask = (Element(1) << bits_to_write) - 1;
            src_val &= mask;
        } else {
            mask = ~Element(0);
        }

        // Clear target bits in dest
        dest[dest_idx] &= ~(mask << dest_shift);

        // Write to dest: shift up to dest position
        dest[dest_idx] |= (src_val << dest_shift);

        // Advance
        bits_remaining -= bits_to_write;

        // Update dest pointers
        dest_shift += bits_to_write;
        if (dest_shift == ELEMENT_WIDTH) {
            dest_shift = 0;
            dest_idx++;
        }

        // Update src pointers
        src_shift += bits_to_write;
        if (src_shift >= ELEMENT_WIDTH) {
            src_shift %= ELEMENT_WIDTH;
            src_idx++;
        }
    }
}

void BitVector::to(DataLocation location)
{
    VELODB_ASSERT_MSG(location != DataLocation::VIEW, "Cannot move data to VIEW");
    if (location_ != location) {
        if (location_ == DataLocation::CUDA) {
            PROFILE_SCOPE("BitVector D2H Transfer");
            Element* host_data;
            auto& stream = CudaStream::getD2HStream();
            CHECKED_CALL_THROW(cudaMallocHost(&host_data, element_capacity_ * sizeof(Element)));
            CHECKED_CALL_THROW(cudaMemcpyAsync(host_data,
                                               data_,
                                               element_capacity_ * sizeof(Element),
                                               cudaMemcpyDeviceToHost,
                                               stream.get()));
            stream.synchronize();
            cudaFree(data_);
            data_ = host_data;
        } else {
            PROFILE_SCOPE("BitVector H2D Transfer");
            Element* device_data;
            auto& stream = CudaStream::getH2DStream();
            CHECKED_CALL_THROW(cudaMalloc(&device_data, element_capacity_ * sizeof(Element)));
            CHECKED_CALL_THROW(cudaMemcpyAsync(device_data,
                                               data_,
                                               element_capacity_ * sizeof(Element),
                                               cudaMemcpyHostToDevice,
                                               stream.get()));
            stream.synchronize();
            if (location_ == DataLocation::HOST) {
                cudaFreeHost(data_);
            }
            data_ = device_data;
        }
        location_ = location;
    }
}

} // namespace velodb
