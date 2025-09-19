#pragma once

#include "common/copy_traits.hpp"
#include "common/exception.hpp"
#include "cuda/compaction.hpp"
#include "cuda/event.hpp"
#include "cuda/event_pool.hpp"
#include "cuda/helper.hpp"
#include "cuda/stream.hpp"
#include "cuda/stream_pool.hpp"
#include "data/bit_vector.hpp"
#include "data/data_location.hpp"
#include "data/type_traits.hpp"
#include "data/value.hpp"
#include "expression/expression.hpp"

#include <utility>

#include <cuda_runtime.h>

namespace velodb {

template <typename T>
class ValueVector : private NonCopyable {
public:
    using DType = std::decay_t<T>;
    using VType = VTypeOfD<DType>;

    ValueVector(size_t capacity, DataLocation location = DataLocation::HOST)
        : data_(nullptr)
        , size_(0)
        , capacity_(capacity)
        , null_mask_(capacity)
        , location_(location)
    {
        if (location == DataLocation::HOST) {
            CHECKED_CALL_THROW(cudaMallocHost(&data_, capacity_ * sizeof(DType)));
        } else if (location == DataLocation::CUDA) {
            CHECKED_CALL_THROW(cudaMalloc(&data_, capacity_ * sizeof(DType)));
        } else {
            VELODB_THROW(ExecutionError, "Invalid data location");
        }
    }

    // Move constructor
    ValueVector(ValueVector&& other) noexcept
        : data_(other.data_)
        , size_(other.size_)
        , capacity_(other.capacity_)
        , null_mask_(std::move(other.null_mask_))
        , location_(other.location_)
    {
        // Reset the source object to a valid but empty state
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
        other.location_ = DataLocation::HOST;
    }

    // Move assignment operator
    ValueVector& operator=(ValueVector&& other) noexcept
    {
        if (this != &other) {
            // Clean up current resources
            if (location_ != DataLocation::VIEW && data_ != nullptr) {
                if (location_ == DataLocation::HOST) {
                    cudaFreeHost(data_);
                } else {
                    cudaFree(data_);
                }
            }

            // Move data from other
            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            null_mask_ = std::move(other.null_mask_);
            location_ = other.location_;

            // Reset the source object
            other.data_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
            other.location_ = DataLocation::HOST;
        }
        return *this;
    }

    ~ValueVector()
    {
        if (location_ == DataLocation::VIEW) {
            return; // View mode does not possess ownership
        }
        if (data_ != nullptr) {
            if (location_ == DataLocation::HOST) {
                cudaFreeHost(data_);
            } else {
                cudaFree(data_);
            }
        }
    }

    size_t size() const { return size_; }

    Value get(size_t index) const
    {
        VELODB_ASSERT_MSG(location_ != DataLocation::CUDA, "Cannot access data on device");
        VELODB_ASSERT_MSG(index < size_, "Index out of range");
        if (null_mask_.get(index)) {
            return Value::createNull(dTypeId<DType>);
        }
        return Value(dTypeId<DType>, VType(data_[index]));
    }

    Result<EventPool::EventHandle> to(DataLocation location)
    {
        VELODB_ASSERT_MSG(location != DataLocation::VIEW, "Cannot move data to VIEW");
        auto handle_result = EventPool::instance().acquire();
        if (location_ != location) {
            if (!handle_result) {
                return Result<EventPool::EventHandle>::failure(handle_result.error());
            }
            if (location_ == DataLocation::CUDA) {
                DType* host_data;
                auto& stream = CudaStream::getD2HStream();
                CHECKED_CALL_THROW(cudaMallocHost(&host_data, capacity_ * sizeof(DType)));
                CHECKED_CALL_THROW(
                    cudaMemcpyAsync(host_data, data_, capacity_ * sizeof(DType), cudaMemcpyDeviceToHost, stream.get()));
                stream.recordEvent(*handle_result.value());
                cudaFree(data_);
                data_ = host_data;
            } else {
                DType* device_data;
                auto& stream = CudaStream::getH2DStream();
                CHECKED_CALL_THROW(cudaMalloc(&device_data, capacity_ * sizeof(DType)));
                CHECKED_CALL_THROW(cudaMemcpyAsync(device_data,
                                                   data_,
                                                   capacity_ * sizeof(DType),
                                                   cudaMemcpyHostToDevice,
                                                   stream.get()));
                stream.recordEvent(*handle_result.value());
                if (location_ == DataLocation::HOST) {
                    cudaFreeHost(data_);
                }
                data_ = device_data;
            }
            location_ = location;
        } else {
            handle_result.value()->markCompleted();
        }
        return handle_result;
    }

    DataLocation location() const { return location_; }

    const DType* data() const { return data_; }

    void resize(size_t new_size, DType value = DType())
    {
        VELODB_ASSERT_MSG(location_ == DataLocation::HOST, "Cannot resize non-host data");
        if (new_size > capacity_) {
            size_t new_capacity = (new_size - capacity_ + 15) / 16 * 16 + capacity_;
            reserve(new_capacity);
        }

        if (new_size > size_) {
            std::fill(data_ + size_, data_ + new_size, value);
        }

        null_mask_.resize(new_size);
        size_ = new_size;
    }

    void reserve(size_t new_capacity)
    {
        if (new_capacity > capacity_) {
            if (location_ == DataLocation::HOST) {
                DType* new_data;
                CHECKED_CALL_THROW(cudaMallocHost(&new_data, new_capacity * sizeof(DType)));
                std::copy(data_, data_ + size_, new_data);
                cudaFreeHost(data_);
                data_ = new_data;
                capacity_ = new_capacity;
            } else if (location_ == DataLocation::CUDA) {
                DType* new_data;
                CHECKED_CALL_THROW(cudaMalloc(&new_data, new_capacity * sizeof(DType)));
                CHECKED_CALL_THROW(cudaMemcpy(new_data, data_, size_ * sizeof(DType), cudaMemcpyDeviceToDevice));
                cudaFree(data_);
                data_ = new_data;
                capacity_ = new_capacity;
            } else {
                VELODB_THROW(ExecutionError, "Cannot reserve data on VIEW");
            }
        }
    }

    void append(const DType& value)
    {
        if (size_ >= capacity_) {
            reserve(capacity_ * 2);
        }
        null_mask_.resize(size_ + 1);
        data_[size_] = value;
        size_++;
    }

    void append(DType&& value)
    {
        if (size_ >= capacity_) {
            reserve(capacity_ * 2);
        }
        null_mask_.resize(size_ + 1);
        data_[size_] = std::move(value);
        size_++;
    }

    ValueVector slice(size_t start, size_t end) const
    {
        VELODB_ASSERT_MSG(start <= end && end <= size_, "Invalid slice range");
        VELODB_ASSERT_MSG(location_ == DataLocation::HOST, "Cannot slice non-host data");

        size_t new_size = end - start;
        return ValueVector(data_ + start, new_size, capacity_, null_mask_.slice(start, end));
    }

    ValueVector tryOwn()
    {
        auto ret = ValueVector(data_, size_, capacity_, null_mask_);
        if (location_ != DataLocation::VIEW) {
            // Transfer ownership
            ret.location_ = location_;
            location_ = DataLocation::VIEW;
        }
        return ret;
    }

    ValueVector splitFront(size_t size)
    {
        VELODB_ASSERT_MSG(location_ != DataLocation::VIEW, "Cannot split a VIEW data source");

        // Create a new ValueVector for the second portion with correct capacity
        if (size > size_) {
            size = size_;
        }
        size_t remaining_size = size_ - size;
        ValueVector split_vector(/* capacity = */ size, location_);

        if (location_ == DataLocation::HOST) {
            std::move(data_, data_ + size, split_vector.data_);
            if (remaining_size > 0) {
                std::move(data_ + size, data_ + size_, data_);
            }
        } else {
            cudaMemcpy(split_vector.data_, data_, size * sizeof(DType), cudaMemcpyDeviceToDevice);
            // TODO: Use memmove semantics to handle overlapping regions
            if (remaining_size > 0) {
                cudaMemcpy(data_, data_ + size, remaining_size * sizeof(DType), cudaMemcpyDeviceToDevice);
            }
        }
        split_vector.null_mask_ = null_mask_.slice(0, size);
        split_vector.size_ = size;

        null_mask_ = null_mask_.slice(size, size_);
        size_ = remaining_size;

        return split_vector;
    }

    void appendMultiple(const ValueVector& other, const ValueVector<uint8_t>& mask)
    {
        VELODB_ASSERT_MSG(location_ == DataLocation::CUDA && other.location_ == DataLocation::CUDA
                              && mask.location() == DataLocation::CUDA,
                          "Filter compaction must happen on CUDA");
        if (size_ + other.size_ > capacity_) {
            reserve(size_ + other.capacity_);
        }
        auto stream_handle = StreamPool::instance().acquire().value();
        size_t num_added = filter_compact(data_ + size_, other.data_, mask.data(), other.size_, stream_handle->get());
        size_ += num_added;
        stream_handle.release();
    }

    void appendMultiple(const ValueVector& other)
    {
        VELODB_ASSERT_MSG(location_ == other.location_ && location_ != DataLocation::VIEW,
                          "Append must happen on same non-VIEW location");
        if (size_ + other.size_ > capacity_) {
            reserve(size_ + other.capacity_);
        }
        if (location_ == DataLocation::HOST) {
            std::copy(other.data_, other.data_ + other.size_, data_ + size_);
        } else {
            CHECKED_CALL_THROW(
                cudaMemcpy(data_ + size_, other.data_, other.size_ * sizeof(DType), cudaMemcpyDeviceToDevice));
        }
        null_mask_.append(other.null_mask_);
        size_ += other.size_;
    }

    static ValueVector buildFrom(std::vector<Value>&& data, DataLocation location = DataLocation::HOST)
    {
        size_t n = data.size();
        ValueVector vec(n, location);
        size_t i = 0;
        for (auto&& value : data) {
            if (value.isNull()) {
                vec.null_mask_.set(i);
            } else {
                vec.data_[i] = static_cast<DType>(value.get<VType>());
            }
            i++;
        }
        vec.size_ = n;
        return vec;
    }

private:
    DType* data_;
    size_t size_;
    size_t capacity_;
    BitVector null_mask_;
    DataLocation location_;

    // Internal constructor
    ValueVector(DType* data, size_t size, size_t capacity, BitVector null_mask)
        : data_(data)
        , size_(size)
        , capacity_(capacity)
        , null_mask_(std::move(null_mask))
        , location_(DataLocation::VIEW)
    {
    }
};

// Template specialization for strings (size_t -> StringVector functionality)
// Implementation moved to value_vector.cpp
template <>
class ValueVector<size_t> : private NonCopyable {
public:
    using DType = size_t;
    using VType = OrdinalString;

    ValueVector(size_t capacity, DataLocation location = DataLocation::HOST);
    ValueVector(ValueVector&& other) noexcept;
    ValueVector& operator=(ValueVector&& other) noexcept;
    ~ValueVector();

    size_t size() const;
    Value get(size_t index) const;
    const DType* data() const;
    void ensureOrdinal(VType& value, ComparisonType comp) const;

    Result<EventPool::EventHandle> to(DataLocation location);
    DataLocation location() const;

    void resize(size_t new_size, DType value = DType());
    void reserve(size_t new_capacity);
    void append(const DType& value);
    void append(DType&& value);

    ValueVector slice(size_t start, size_t end) const;
    ValueVector tryOwn();
    ValueVector splitFront(size_t size);
    void appendMultiple(const ValueVector<size_t>& other, const ValueVector<uint8_t>& mask);
    void appendMultiple(const ValueVector<size_t>& other);

    static ValueVector buildFrom(std::vector<Value>&& data, DataLocation location = DataLocation::HOST);

private:
    DType* data_;
    size_t size_;
    size_t capacity_;
    BitVector null_mask_;
    DataLocation location_;
    std::shared_ptr<std::vector<std::string>> ordered_strings_;

    // Internal constructor
    ValueVector(DType* data,
                size_t size,
                size_t capacity,
                BitVector null_mask,
                const std::shared_ptr<std::vector<std::string>>& ordered_strings);
};

// Type alias for backward compatibility
using StringVector = ValueVector<size_t>;

} // namespace velodb
