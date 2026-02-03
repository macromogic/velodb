#include "data/bit_vector.hpp"

#include "common/exception.hpp"
#include "common/profiler.hpp"
#include "cuda/helper.hpp"
#include "cuda/host_memory_pool.hpp"
#include "cuda/pageable_memory_pool.hpp"
#include "cuda/staged_transfer.hpp"
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
    , data_(nullptr)
{
    if (location_ == DataLocation::HOST || location_ == DataLocation::HOST_PINNED) {
        data_ = static_cast<Element*>(HostMemoryPool::getInstance().allocate(element_capacity_ * sizeof(Element)));
        std::fill_n(data_, element_capacity_, Element(0));
        location_ = DataLocation::HOST_PINNED; // Normalize HOST to HOST_PINNED
    } else if (location_ == DataLocation::HOST_PAGEABLE) {
        data_ = static_cast<Element*>(PageableMemoryPool::getInstance().allocate(element_capacity_ * sizeof(Element)));
        std::fill_n(data_, element_capacity_, Element(0));
    } else {
        auto stream_handle = StreamPool::getInstance().acquire().value();
        CHECKED_CALL_THROW(cudaMallocAsync(&data_, element_capacity_ * sizeof(Element), stream_handle->get()));
        CHECKED_CALL_THROW(cudaMemsetAsync(data_, 0, element_capacity_ * sizeof(Element), stream_handle->get()));
    }
}

BitVector::BitVector(Element* data, size_t size, size_t capacity, DataLocation location)
    : size_(size)
    , element_capacity_(capacity)
    , location_(location)
    , data_(data)
{
    if (!data_) {
        // Allocate if not provided
        if (location_ == DataLocation::HOST || location_ == DataLocation::HOST_PINNED) {
            data_ = static_cast<Element*>(HostMemoryPool::getInstance().allocate(element_capacity_ * sizeof(Element)));
            std::fill_n(data_, element_capacity_, Element(0));
            location_ = DataLocation::HOST_PINNED;
        } else if (location_ == DataLocation::HOST_PAGEABLE) {
            data_ = static_cast<Element*>(
                PageableMemoryPool::getInstance().allocate(element_capacity_ * sizeof(Element)));
            std::fill_n(data_, element_capacity_, Element(0));
        } else {
            auto stream_handle = StreamPool::getInstance().acquire().value();
            CHECKED_CALL_THROW(cudaMallocAsync(&data_, element_capacity_ * sizeof(Element), stream_handle->get()));
            CHECKED_CALL_THROW(cudaMemsetAsync(data_, 0, element_capacity_ * sizeof(Element), stream_handle->get()));
        }
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

BitVector BitVector::cloneImpl() const
{
    // For views, clone to owned memory
    DataLocation clone_location = location_;
    if (location_ == DataLocation::VIEW) {
        clone_location = DataLocation::HOST_PINNED;
    } else if (location_ == DataLocation::CUDA_VIEW) {
        clone_location = DataLocation::CUDA;
    }

    BitVector copy(size_, clone_location);
    switch (location_) {
    case DataLocation::HOST_PINNED: // HOST is an alias for HOST_PINNED
    case DataLocation::HOST_PAGEABLE:
    case DataLocation::VIEW:
        std::copy(data_, data_ + element_capacity_, copy.data_);
        break;
    case DataLocation::CUDA:
    case DataLocation::CUDA_VIEW: {
        auto stream_handle = StreamPool::getInstance().acquire().value();
        CHECKED_CALL_THROW(cudaMemcpyAsync(copy.data_,
                                           data_,
                                           element_capacity_ * sizeof(Element),
                                           cudaMemcpyDeviceToDevice,
                                           stream_handle->get()));
        stream_handle->synchronize();
        break;
    }
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
    case DataLocation::HOST_PINNED: // HOST is an alias for HOST_PINNED
        HostMemoryPool::getInstance().deallocate(data_, element_capacity_ * sizeof(Element));
        break;
    case DataLocation::HOST_PAGEABLE:
        PageableMemoryPool::getInstance().deallocate(data_, element_capacity_ * sizeof(Element));
        break;
    case DataLocation::CUDA: {
        auto stream_handle = StreamPool::getInstance().acquire().value();
        cudaFreeAsync(data_, stream_handle->get());
        stream_handle->synchronize();
        break;
    }
    case DataLocation::VIEW:
    case DataLocation::CUDA_VIEW:
        // Non-owning views don't free memory
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
        auto stream_handle = StreamPool::getInstance().acquire().value();
        CHECKED_CALL_THROW(cudaMemsetAsync(data_ + index, 1, sizeof(Element), stream_handle->get()));
        stream_handle->synchronize();
    } else {
        data_[index] = 1;
    }
}

void BitVector::unset(size_t index)
{
    VELODB_ASSERT_MSG(location_ != DataLocation::VIEW, "Cannot modify VIEW BitVector");
    VELODB_ASSERT_MSG(index < size_, "Invalid index");
    if (location_ == DataLocation::CUDA) {
        auto stream_handle = StreamPool::getInstance().acquire().value();
        CHECKED_CALL_THROW(cudaMemsetAsync(data_ + index, 0, sizeof(Element), stream_handle->get()));
        stream_handle->synchronize();
    } else {
        data_[index] = 0;
    }
}

bool BitVector::get(size_t index) const
{
    VELODB_ASSERT_MSG(index < size_, "Invalid index");
    if (location_ == DataLocation::CUDA) {
        auto stream_handle = StreamPool::getInstance().acquire().value();
        Element value;
        CHECKED_CALL_THROW(
            cudaMemcpyAsync(&value, data_ + index, sizeof(Element), cudaMemcpyDeviceToHost, stream_handle->get()));
        stream_handle->synchronize();
        return value != 0;
    } else {
        return data_[index] != 0;
    }
}

void BitVector::resize(size_t new_size)
{
    VELODB_ASSERT_MSG(isHostLocation(location_), "Cannot resize non-host BitVector");
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
        if (location_ == DataLocation::CUDA) {
            auto stream_handle = StreamPool::getInstance().acquire().value();
            CHECKED_CALL_THROW(
                cudaMallocAsync(&new_data, new_element_capacity * sizeof(Element), stream_handle->get()));
            CHECKED_CALL_THROW(cudaMemcpyAsync(new_data,
                                               data_,
                                               element_capacity_ * sizeof(Element),
                                               cudaMemcpyDeviceToDevice,
                                               stream_handle->get()));
            CHECKED_CALL_THROW(cudaMemsetAsync(new_data + element_capacity_,
                                               0,
                                               (new_element_capacity - element_capacity_) * sizeof(Element),
                                               stream_handle->get()));
            CHECKED_CALL_THROW(cudaFreeAsync(data_, stream_handle->get()));
            stream_handle->synchronize();
        } else if (location_ == DataLocation::HOST_PAGEABLE) {
            auto& pageable_pool = PageableMemoryPool::getInstance();
            new_data = static_cast<Element*>(pageable_pool.allocate(new_element_capacity * sizeof(Element)));
            if (data_) {
                std::copy(data_, data_ + size_, new_data);
            }
            std::fill_n(new_data + size_, new_element_capacity - size_, Element(0));
            pageable_pool.deallocate(data_, element_capacity_ * sizeof(Element));
        } else {
            auto& host_memory_pool = HostMemoryPool::getInstance();
            new_data = static_cast<Element*>(host_memory_pool.allocate(new_element_capacity * sizeof(Element)));
            if (data_) {
                std::copy(data_, data_ + size_, new_data);
            }
            // Initialize the rest (padding) just in case, though usually only size_ matters
            std::fill_n(new_data + size_, new_element_capacity - size_, Element(0));
            host_memory_pool.deallocate(data_, element_capacity_ * sizeof(Element));
        }
        data_ = new_data;
        element_capacity_ = new_element_capacity;
    }
}

BitVector BitVector::slice(size_t start, size_t end) const
{
    VELODB_ASSERT_MSG(start <= end && end <= size_, "Invalid slice range");

    size_t slice_bits = end - start;

    if (isCudaLocation(location_)) {
        // CUDA data: return a CUDA_VIEW
        return BitVector(data_ + start, slice_bits, element_capacity_ - start, DataLocation::CUDA_VIEW);
    } else {
        // HOST data: return a VIEW (copy bits for now, could optimize to VIEW later)
        BitVector result(slice_bits);
        if (slice_bits > 0) {
            copyBits(result.data_, 0, data_, start, slice_bits);
        }
        return result;
    }
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
    VELODB_ASSERT_MSG(location != DataLocation::VIEW && location != DataLocation::CUDA_VIEW,
                      "Cannot move data to VIEW or CUDA_VIEW");

    // Normalize target location
    DataLocation target = (location == DataLocation::HOST) ? DataLocation::HOST_PINNED : location;

    // Handle VIEW: need to copy data first to take ownership
    // Optimize: copy directly to target location instead of always going through HOST_PINNED
    if (location_ == DataLocation::VIEW) {
        if (target == DataLocation::CUDA) {
            // VIEW -> CUDA: use pipelined staged transfer (VIEW data is in pageable memory)
            auto stream_handle = StreamPool::getInstance().acquire().value();
            Element* device_data;
            CHECKED_CALL_THROW(
                cudaMallocAsync(&device_data, element_capacity_ * sizeof(Element), stream_handle->get()));
            stream_handle->synchronize();
            StagedTransfer::toDevicePipelined(device_data, data_, element_capacity_ * sizeof(Element));
            data_ = device_data;
            location_ = DataLocation::CUDA;
        } else if (target == DataLocation::HOST_PAGEABLE) {
            // VIEW -> HOST_PAGEABLE: copy to pageable memory
            Element* new_data = static_cast<Element*>(
                PageableMemoryPool::getInstance().allocate(element_capacity_ * sizeof(Element)));
            std::copy(data_, data_ + element_capacity_, new_data);
            data_ = new_data;
            location_ = DataLocation::HOST_PAGEABLE;
        } else {
            // VIEW -> HOST_PINNED: copy to pinned memory
            Element* new_data = static_cast<Element*>(
                HostMemoryPool::getInstance().allocate(element_capacity_ * sizeof(Element)));
            std::copy(data_, data_ + element_capacity_, new_data);
            data_ = new_data;
            location_ = DataLocation::HOST_PINNED;
        }
    } else if (location_ == DataLocation::CUDA_VIEW) {
        // Copy data to owned CUDA memory with proper padding for oblivious transfer
        // For VIEW, only transfer size_ elements
        auto stream_handle = StreamPool::getInstance().acquire().value();
        size_t padded_capacity = nextPow2(size_);
        Element* new_data;
        CHECKED_CALL_THROW(cudaMallocAsync(&new_data, padded_capacity * sizeof(Element), stream_handle->get()));
        CHECKED_CALL_THROW(
            cudaMemcpyAsync(new_data, data_, size_ * sizeof(Element), cudaMemcpyDeviceToDevice, stream_handle->get()));
        // Zero out padding region
        if (padded_capacity > size_) {
            CHECKED_CALL_THROW(cudaMemsetAsync(new_data + size_,
                                               0,
                                               (padded_capacity - size_) * sizeof(Element),
                                               stream_handle->get()));
        }
        stream_handle->synchronize();
        data_ = new_data;
        element_capacity_ = padded_capacity;
        location_ = DataLocation::CUDA;
    }

    // Check if we've already reached target
    DataLocation current = (location_ == DataLocation::HOST) ? DataLocation::HOST_PINNED : location_;

    if (current == target) {
        return;
    }

    auto stream_handle = StreamPool::getInstance().acquire().value();

    if (current == DataLocation::CUDA) {
        // CUDA -> HOST_PINNED or CUDA -> HOST_PAGEABLE

        if (target == DataLocation::HOST_PINNED) {
            // CUDA -> HOST_PINNED: direct DMA
            Element* host_data = static_cast<Element*>(
                HostMemoryPool::getInstance().allocate(element_capacity_ * sizeof(Element)));
            CHECKED_CALL_THROW(cudaMemcpyAsync(host_data,
                                               data_,
                                               element_capacity_ * sizeof(Element),
                                               cudaMemcpyDeviceToHost,
                                               stream_handle->get()));
            CHECKED_CALL_THROW(cudaFreeAsync(data_, stream_handle->get()));
            stream_handle->synchronize();
            data_ = host_data;
        } else {
            // CUDA -> HOST_PAGEABLE: staged transfer
            Element* pageable_data = static_cast<Element*>(
                PageableMemoryPool::getInstance().allocate(element_capacity_ * sizeof(Element)));
            StagedTransfer::toHost(pageable_data, data_, element_capacity_ * sizeof(Element));
            CHECKED_CALL_THROW(cudaFreeAsync(data_, stream_handle->get()));
            stream_handle->synchronize();
            data_ = pageable_data;
        }
        location_ = target;
    } else if (target == DataLocation::CUDA) {
        // HOST_PINNED -> CUDA or HOST_PAGEABLE -> CUDA
        Element* device_data;
        CHECKED_CALL_THROW(cudaMallocAsync(&device_data, element_capacity_ * sizeof(Element), stream_handle->get()));

        if (current == DataLocation::HOST_PINNED) {
            // HOST_PINNED -> CUDA: direct DMA
            CHECKED_CALL_THROW(cudaMemcpyAsync(device_data,
                                               data_,
                                               element_capacity_ * sizeof(Element),
                                               cudaMemcpyHostToDevice,
                                               stream_handle->get()));
            HostMemoryPool::getInstance().deallocate(data_, element_capacity_ * sizeof(Element));
        } else {
            // HOST_PAGEABLE -> CUDA: use pipelined staged transfer for better throughput
            stream_handle->synchronize(); // Ensure device_data is allocated
            StagedTransfer::toDevicePipelined(device_data, data_, element_capacity_ * sizeof(Element));
            PageableMemoryPool::getInstance().deallocate(data_, element_capacity_ * sizeof(Element));
        }
        stream_handle->synchronize();
        data_ = device_data;
        location_ = DataLocation::CUDA;
    } else {
        // HOST_PINNED <-> HOST_PAGEABLE
        if (current == DataLocation::HOST_PINNED && target == DataLocation::HOST_PAGEABLE) {
            Element* pageable_data = static_cast<Element*>(
                PageableMemoryPool::getInstance().allocate(element_capacity_ * sizeof(Element)));
            std::copy(data_, data_ + element_capacity_, pageable_data);
            HostMemoryPool::getInstance().deallocate(data_, element_capacity_ * sizeof(Element));
            data_ = pageable_data;
            location_ = DataLocation::HOST_PAGEABLE;
        } else {
            Element* pinned_data = static_cast<Element*>(
                HostMemoryPool::getInstance().allocate(element_capacity_ * sizeof(Element)));
            std::copy(data_, data_ + element_capacity_, pinned_data);
            PageableMemoryPool::getInstance().deallocate(data_, element_capacity_ * sizeof(Element));
            data_ = pinned_data;
            location_ = DataLocation::HOST_PINNED;
        }
    }
}

} // namespace velodb
