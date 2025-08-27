#pragma once

#include "common/copy_traits.hpp"
#include "common/exception.hpp"
#include "cuda/helper.hpp"
#include "cuda/stream.hpp"
#include "data/bit_vector.hpp"
#include "data/data_type.hpp"
#include "data/fixed_string.hpp"
#include "data/type_traits.hpp"
#include "data/value.hpp"

#include <functional>
#include <optional>
#include <utility>

#include <cuda_runtime.h>

namespace velodb {

enum class DataLocation {
    HOST,
    CUDA,
};

inline auto format_as(DataLocation location)
{
    switch (location) {
    case DataLocation::HOST:
        return "HOST";
    case DataLocation::CUDA:
        return "CUDA";
    default:
        return "UNKNOWN";
    }
}

template <typename T>
class ValueVectorImpl : private NonCopyable {
public:
    using DType = std::decay_t<T>;
    using VType = VTypeOfD<DType>;

    ValueVectorImpl(size_t capacity)
        : data_(nullptr)
        , size_(0)
        , capacity_(capacity)
        , valid_mask_(capacity)
        , location_(DataLocation::HOST)
    {
        // Just allocate memory - don't construct objects until needed
        CHECKED_CALL_THROW(cudaMallocHost(&data_, capacity_ * sizeof(T)));
    }

    // Constructor for FixedString with max_length
    ValueVectorImpl(size_t capacity, size_t max_length)
        : data_(nullptr)
        , size_(0)
        , capacity_(capacity)
        , valid_mask_(capacity)
        , location_(DataLocation::HOST)
        , max_length_(max_length)
    {
        static_assert(std::is_same_v<T, FixedString>, "max_length constructor only for FixedString");
        // Just allocate memory - don't construct objects until needed
        CHECKED_CALL_THROW(cudaMallocHost(&data_, capacity_ * sizeof(T)));
    }

    // Move constructor
    ValueVectorImpl(ValueVectorImpl&& other) noexcept
        : data_(other.data_)
        , size_(other.size_)
        , capacity_(other.capacity_)
        , valid_mask_(std::move(other.valid_mask_))
        , location_(other.location_)
        , max_length_(other.max_length_)
    {
        // Reset the source object to a valid but empty state
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
        other.location_ = DataLocation::HOST;
        other.max_length_ = 0;
    }

    ~ValueVectorImpl()
    {
        if (data_ != nullptr) {
            if constexpr (std::is_same_v<T, FixedString>) {
                // Call destructors only for FixedString objects that were actually constructed
                // (only up to size_, since we use lazy construction)
                for (size_t i = 0; i < size_; ++i) {
                    (data_ + i)->~T();
                }
            }
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
        if (location_ != DataLocation::HOST) {
            VELODB_THROW(ExecutionError, "Cannot access data on device");
        }
        if (index >= size_) {
            VELODB_THROW(ExecutionError, "Index out of range");
        }
        if (!valid_mask_.get(index)) {
            return Value::createNull(dTypeId<DType>);
        }
        return Value(dTypeId<DType>, VType(data_[index]));
    }

    void to(DataLocation location)
    {
        if (location_ != location) {
            if (location_ == DataLocation::HOST) {
                DType* device_data;
                auto& stream = CudaStream::getH2DStream();
                CHECKED_CALL_THROW(cudaMalloc(&device_data, capacity_ * sizeof(DType)));
                CHECKED_CALL_THROW(cudaMemcpyAsync(device_data,
                                                   data_,
                                                   capacity_ * sizeof(DType),
                                                   cudaMemcpyHostToDevice,
                                                   stream.get()));
                stream.synchronize();
                cudaFreeHost(data_);
                data_ = device_data;
            } else {
                DType* host_data;
                auto& stream = CudaStream::getD2HStream();
                CHECKED_CALL_THROW(cudaMallocHost(&host_data, capacity_ * sizeof(DType)));
                CHECKED_CALL_THROW(
                    cudaMemcpyAsync(host_data, data_, capacity_ * sizeof(DType), cudaMemcpyDeviceToHost, stream.get()));
                stream.synchronize();
                cudaFree(data_);
                data_ = host_data;
            }
            location_ = location;
        }
    }

    DataLocation location() const { return location_; }

    void resize(size_t new_size, DType value = DType())
    {
        if (location_ != DataLocation::HOST) {
            VELODB_THROW(ExecutionError, "Cannot resize data on device");
        }
        if (new_size > capacity_) {
            size_t new_capacity = (new_size - capacity_ + 15) / 16 * 16 + capacity_;
            reserve(new_capacity);
        }

        if (new_size > size_) {
            if constexpr (std::is_same_v<T, FixedString>) {
                // For FixedString, construct new objects lazily
                for (size_t i = size_; i < new_size; ++i) {
                    if (max_length_ > 0) {
                        new (data_ + i) T(max_length_);
                    } else {
                        new (data_ + i) T();
                    }
                }
            } else {
                // For other types, use fill
                std::fill(data_ + size_, data_ + new_size, value);
            }
        } else if (new_size < size_) {
            if constexpr (std::is_same_v<T, FixedString>) {
                // Destroy objects that are no longer needed
                for (size_t i = new_size; i < size_; ++i) {
                    (data_ + i)->~T();
                }
            }
        }

        valid_mask_.resize(new_size);
        size_ = new_size;
    }

    void reserve(size_t new_capacity)
    {
        if (new_capacity > capacity_) {
            if (location_ == DataLocation::HOST) {
                T* new_data;
                CHECKED_CALL_THROW(cudaMallocHost(&new_data, new_capacity * sizeof(T)));

                if constexpr (std::is_same_v<T, FixedString>) {
                    // For FixedString, copy construct only the existing objects (up to size_)
                    size_t copied_count = 0;
                    try {
                        for (size_t i = 0; i < size_; ++i) {
                            new (new_data + i) T(data_[i]);
                            copied_count++;
                        }

                        // Destroy old objects (only up to size_)
                        for (size_t i = 0; i < size_; ++i) {
                            (data_ + i)->~T();
                        }
                    } catch (...) {
                        // Clean up any new objects that were constructed
                        for (size_t i = 0; i < copied_count; ++i) {
                            (new_data + i)->~T();
                        }
                        cudaFreeHost(new_data);
                        throw;
                    }
                } else {
                    // For non-FixedString types, use copy (only up to size_)
                    std::copy(data_, data_ + size_, new_data);
                }

                cudaFreeHost(data_);
                data_ = new_data;
                capacity_ = new_capacity;
            } else {
                T* new_data;
                CHECKED_CALL_THROW(cudaMalloc(&new_data, new_capacity * sizeof(T)));
                CHECKED_CALL_THROW(cudaMemcpy(new_data, data_, size_ * sizeof(T), cudaMemcpyDeviceToDevice));
                cudaFree(data_);
                data_ = new_data;
                capacity_ = new_capacity;
            }
        }
    }

    void clear()
    {
        if (location_ != DataLocation::HOST) {
            VELODB_THROW(ExecutionError, "Cannot clear data on device");
        }
        valid_mask_.clear();
        size_ = 0;
    }

    void append(const DType& value)
    {
        if (size_ >= capacity_) {
            reserve(capacity_ * 2);
        }
        valid_mask_.resize(size_ + 1);
        valid_mask_.set(size_);

        if constexpr (std::is_same_v<T, FixedString>) {
            // For FixedString, construct the object at the new position
            new (data_ + size_) T(value);
        } else {
            data_[size_] = value;
        }
        size_++;
    }

    void append(DType&& value)
    {
        if (size_ >= capacity_) {
            reserve(capacity_ * 2);
        }
        valid_mask_.resize(size_ + 1);
        valid_mask_.set(size_);

        if constexpr (std::is_same_v<T, FixedString>) {
            // For FixedString, construct the object at the new position
            new (data_ + size_) T(std::move(value));
        } else {
            data_[size_] = std::move(value);
        }
        size_++;
    }

private:
    DType* data_;
    size_t size_;
    size_t capacity_;
    BitVector valid_mask_;
    DataLocation location_;
    size_t max_length_ = 0; // For FixedString, stores max_length for lazy construction
};

class ValueVector : private NonCopyable {
public:
    using DataSource = std::variant<ValueVectorImpl<uint8_t>,
                                    ValueVectorImpl<int8_t>,
                                    ValueVectorImpl<int16_t>,
                                    ValueVectorImpl<int32_t>,
                                    ValueVectorImpl<int64_t>,
                                    ValueVectorImpl<float>,
                                    ValueVectorImpl<double>,
                                    ValueVectorImpl<FixedString>>;

    explicit ValueVector(std::unique_ptr<DataType> type, size_t initial_capacity = 16);
    ValueVector(ValueVector&& other) = default;
    ValueVector& operator=(ValueVector&& other) = default;

    size_t size() const;
    Value get(size_t index) const;
    Value operator[](size_t index) const;
    DataLocation location() const;
    void reserve(size_t new_capacity);
    void resize(size_t new_size, const Value& value);
    void clear();
    void append(const Value& value);
    void append(Value&& value);

private:
    std::unique_ptr<DataType> type_;
    DataSource data_source_;
};

} // namespace velodb
