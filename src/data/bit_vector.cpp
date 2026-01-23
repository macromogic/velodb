#include "data/bit_vector.hpp"

#include "common/exception.hpp"
#include "common/profiler.hpp"
#include "cuda/allocator.hpp"
#include "cuda/helper.hpp"
#include "cuda/stream.hpp"
#include "cuda/stream_pool.hpp"
#include "data/data_location.hpp"

#include <algorithm>
#include <cstring>

#include <cuda_runtime.h>

namespace velodb {

BitVector::BitVector(size_t num_bits, DataLocation location)
    : size_(num_bits)
    , element_capacity_(std::max(nextPow2(num_bits), 1ul))
    , location_(location)
    , data_(MemoryAllocator::allocate<Element>(location_, element_capacity_))
{
    if (location_ == DataLocation::HOST) {
        std::fill_n(data_, element_capacity_, Element(0));
    } else {
        CHECKED_CALL_THROW(cudaMemset(data_, 0, element_capacity_ * sizeof(Element)));
    }
}

// BitVector::BitVector(const BitVector& other)
//     : size_(other.size_)
//     , element_capacity_(other.element_capacity_)
//     , location_(other.location_)
//     , data_(nullptr)
// {
//     switch (location_) {
//     case DataLocation::HOST:
//         data_ = MemoryAllocator::allocate<Element>(DataLocation::HOST, element_capacity_);
//         std::copy(other.data_, other.data_ + element_capacity_, data_);
//         break;
//     case DataLocation::CUDA:
//         data_ = MemoryAllocator::allocate<Element>(DataLocation::CUDA, element_capacity_);
//         CHECKED_CALL_THROW(cudaMemcpy(data_, other.data_, element_capacity_ * sizeof(Element),
//         cudaMemcpyDeviceToDevice)); break;
//     case DataLocation::VIEW:
//         data_ = other.data_;
//         break;
//     default:
//         __builtin_unreachable();
//     }
// }

BitVector::BitVector(BitVector&& other) noexcept
    : size_(other.size_)
    , element_capacity_(other.element_capacity_)
    , location_(other.location_)
    , data_(nullptr)
{
    std::swap(data_, other.data_);
}

// BitVector& BitVector::operator=(const BitVector& other)
// {
//     size_ = other.size_;
//     location_ = other.location_;
//     element_capacity_ = other.element_capacity_;
//     switch (location_) {
//     case DataLocation::HOST:
//         data_ = MemoryAllocator::allocate<Element>(DataLocation::HOST, element_capacity_);
//         std::copy(other.data_, other.data_ + element_capacity_, data_);
//         break;
//     case DataLocation::CUDA:
//         data_ = MemoryAllocator::allocate<Element>(DataLocation::CUDA, element_capacity_);
//         CHECKED_CALL_THROW(cudaMemcpy(data_, other.data_, element_capacity_ * sizeof(Element),
//         cudaMemcpyDeviceToDevice)); break;
//     case DataLocation::VIEW:
//         data_ = other.data_;
//         break;
//     default:
//         __builtin_unreachable();
//     }
//     return *this;
// }

BitVector BitVector::cloneImpl() const
{
    BitVector copy(size_, location_);
    switch (location_) {
    case DataLocation::HOST:
        std::copy(data_, data_ + element_capacity_, copy.data_);
        break;
    case DataLocation::CUDA: {
        auto stream_handler = StreamPool::getInstance().acquire().value();
        CHECKED_CALL_THROW(cudaMemcpyAsync(copy.data_,
                                           data_,
                                           element_capacity_ * sizeof(Element),
                                           cudaMemcpyDeviceToDevice,
                                           stream_handler->get()));
        stream_handler->synchronize();
        break;
    }
    case DataLocation::VIEW:
        copy.data_ = data_;
        break;
    default:
        __builtin_unreachable();
    }
    return copy;
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
    case DataLocation::CUDA:
        MemoryAllocator::deallocate(data_);
        break;
    default:
        break;
    }
}

void BitVector::set(size_t index)
{
    VELODB_ASSERT_MSG(location_ != DataLocation::VIEW, "Cannot modify VIEW BitVector");
    VELODB_ASSERT_MSG(index < size_, "Invalid index");
    if (location_ == DataLocation::CUDA) {
        auto stream_handler = StreamPool::getInstance().acquire().value();
        CHECKED_CALL_THROW(cudaMemsetAsync(data_ + index, 1, sizeof(Element), stream_handler->get()));
        stream_handler->synchronize();
    } else {
        data_[index] = 1;
    }
}

void BitVector::unset(size_t index)
{
    VELODB_ASSERT_MSG(location_ != DataLocation::VIEW, "Cannot modify VIEW BitVector");
    VELODB_ASSERT_MSG(index < size_, "Invalid index");
    if (location_ == DataLocation::CUDA) {
        auto stream_handler = StreamPool::getInstance().acquire().value();
        CHECKED_CALL_THROW(cudaMemsetAsync(data_ + index, 0, sizeof(Element), stream_handler->get()));
        stream_handler->synchronize();
    } else {
        data_[index] = 0;
    }
}

bool BitVector::get(size_t index) const
{
    VELODB_ASSERT_MSG(index < size_, "Invalid index");
    if (location_ == DataLocation::CUDA) {
        auto stream_handler = StreamPool::getInstance().acquire().value();
        Element value;
        CHECKED_CALL_THROW(
            cudaMemcpyAsync(&value, data_ + index, sizeof(Element), cudaMemcpyDeviceToHost, stream_handler->get()));
        stream_handler->synchronize();
        return value != 0;
    } else {
        return data_[index] != 0;
    }
}

void BitVector::resize(size_t new_size)
{
    VELODB_ASSERT_MSG(location_ == DataLocation::HOST, "Cannot resize non-host BitVector");
    reserve(new_size); // Ensure capacity
    if (new_size > size_) {
        // Initialize new elements to 0
        std::fill_n(data_ + size_, new_size - size_, Element(0));
    }
    size_ = new_size;
}

void BitVector::reserve(size_t new_capacity)
{
    VELODB_ASSERT_MSG(location_ != DataLocation::VIEW, "Cannot reserve a VIEW BitVector");
    size_t new_element_capacity = nextPow2(new_capacity);

    if (new_element_capacity > element_capacity_) {
        Element* new_data;
        auto stream_handler = StreamPool::getInstance().acquire().value();
        if (location_ == DataLocation::CUDA) {
            new_data = MemoryAllocator::allocate<Element>(DataLocation::CUDA, new_element_capacity);
            CHECKED_CALL_THROW(cudaMemcpyAsync(new_data,
                                               data_,
                                               element_capacity_ * sizeof(Element),
                                               cudaMemcpyDeviceToDevice,
                                               stream_handler->get()));
            CHECKED_CALL_THROW(cudaMemsetAsync(new_data + element_capacity_,
                                               0,
                                               (new_element_capacity - element_capacity_) * sizeof(Element),
                                               stream_handler->get()));
            stream_handler->synchronize();
        } else {
            new_data = MemoryAllocator::allocate<Element>(DataLocation::HOST, new_element_capacity);
            stream_handler->synchronize();
            if (data_) {
                std::copy(data_, data_ + size_, new_data);
            }
            // Initialize the rest (padding) just in case, though usually only size_ matters
            std::fill_n(new_data + size_, new_element_capacity - size_, Element(0));
        }

        if (data_) {
            MemoryAllocator::deallocate(data_);
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

void BitVector::append(const BitVector& other)
{
    size_t original_size = size_;
    resize(size_ + other.size_);

    copyBits(data_, original_size, other.data_, 0, other.size_);
}

void BitVector::copyBits(Element* dest, size_t dest_offset, const Element* src, size_t src_offset, size_t num_bits)
{
    // Implementation for byte-array is just a memcpy/copy
    // dest_offset, src_offset are indices now, not bit offsets
    if (num_bits > 0) {
        std::memcpy(dest + dest_offset, src + src_offset, num_bits * sizeof(Element));
    }
}

void BitVector::to(DataLocation location)
{
    VELODB_ASSERT_MSG(location != DataLocation::VIEW, "Cannot move data to VIEW");
    if (location_ != location) {
        auto stream_handler = StreamPool::getInstance().acquire().value();
        if (location_ == DataLocation::CUDA) {
            PROFILE_SCOPE("BitVector D2H Transfer");
            // For Byte Vector, capacity is bytes
            Element* host_data = MemoryAllocator::allocate<Element>(DataLocation::HOST,
                                                                    element_capacity_,
                                                                    stream_handler->get());
            CHECKED_CALL_THROW(cudaMemcpyAsync(host_data,
                                               data_,
                                               element_capacity_ * sizeof(Element),
                                               cudaMemcpyDeviceToHost,
                                               stream_handler->get()));
            MemoryAllocator::deallocate(data_, stream_handler->get());
            stream_handler->synchronize();
            data_ = host_data;
        } else {
            PROFILE_SCOPE("BitVector H2D Transfer");
            Element* device_data = MemoryAllocator::allocate<Element>(DataLocation::CUDA,
                                                                      element_capacity_,
                                                                      stream_handler->get());
            CHECKED_CALL_THROW(cudaMemcpyAsync(device_data,
                                               data_,
                                               element_capacity_ * sizeof(Element),
                                               cudaMemcpyHostToDevice,
                                               stream_handler->get()));
            if (location_ == DataLocation::HOST) {
                MemoryAllocator::deallocate(data_, stream_handler->get());
            }
            stream_handler->synchronize();
            data_ = device_data;
        }
        location_ = location;
    }
}

} // namespace velodb
