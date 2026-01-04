#pragma once

#include "common/copy_traits.hpp"
#include "common/exception.hpp"
#include "cuda/compaction.hpp"
#include "cuda/event.hpp"
#include "cuda/helper.hpp"
#include "cuda/materialization.hpp"
#include "cuda/sort.hpp"
#include "cuda/stream.hpp"
#include "cuda/stream_pool.hpp"
#include "data/bit_vector.hpp"
#include "data/data_location.hpp"
#include "data/type_traits.hpp"
#include "data/value.hpp"
#include "expression/expression.hpp"

#include <fmt/ranges.h>

#include <utility>

#include <cuda_runtime.h>

namespace velodb {

// Forward declaration
template <typename VT>
class ValueVector;

template <typename DT, template <typename> class VecT>
class ValueVectorBase : private NonCopyable {
public:
    using DType = DT;
    using VType = VTypeOfD<DType>;
    using ConcreteVector = VecT<VType>;
    using MaskVector = VecT<bool>;
    using IntVector = VecT<int64_t>;

    ValueVectorBase(size_t capacity, DataLocation location = DataLocation::HOST)
        : data_(nullptr)
        , size_(0)
        , capacity_(location == DataLocation::CUDA ? nextPow2(capacity) : capacity)
        , null_mask_(0)
        , location_(location)
    {
        null_mask_.reserve(capacity_);
        if (location == DataLocation::HOST) {
            CHECKED_CALL_THROW(cudaMallocHost(&data_, capacity_ * sizeof(DType)));
        } else if (location == DataLocation::CUDA) {
            CHECKED_CALL_THROW(cudaMalloc(&data_, capacity_ * sizeof(DType)));
            // Zero-initialize padding region to prevent data leaks during oblivious transfer
            CHECKED_CALL_THROW(cudaMemset(data_, 0, capacity_ * sizeof(DType)));
        } else {
            VELODB_THROW(ExecutionError, "Invalid data location");
        }
    }

    // Move constructor
    ValueVectorBase(ValueVectorBase&& other) noexcept
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
    ValueVectorBase& operator=(ValueVectorBase&& other) noexcept
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

    virtual ~ValueVectorBase()
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

    void to(DataLocation location)
    {
        VELODB_ASSERT_MSG(location != DataLocation::VIEW, "Cannot move data to VIEW");
        if (location_ != location) {
            if (location_ == DataLocation::CUDA) {
                // Oblivious transfer: round up to next power of 2 to hide selectivity
                size_t padded_capacity = nextPow2(capacity_);
                VELODB_ASSERT_MSG(capacity_ >= padded_capacity, "Insufficient capacity for oblivious transfer");
                DType* host_data;
                auto& stream = CudaStream::getD2HStream();
                CHECKED_CALL_THROW(cudaMallocHost(&host_data, padded_capacity * sizeof(DType)));
                CHECKED_CALL_THROW(
                    cudaMemcpyAsync(host_data, data_, padded_capacity * sizeof(DType), cudaMemcpyDeviceToHost, stream.get()));
                stream.synchronize();
                cudaFree(data_);
                data_ = host_data;
                capacity_ = padded_capacity;
            } else {
                size_t padded_capacity = nextPow2(capacity_);
                DType* device_data;
                auto& stream = CudaStream::getH2DStream();
                CHECKED_CALL_THROW(cudaMallocAsync(&device_data, padded_capacity * sizeof(DType), stream.get()));
                stream.synchronize();
                CHECKED_CALL_THROW(cudaMemcpyAsync(device_data,
                                                   data_,
                                                   capacity_ * sizeof(DType),
                                                   cudaMemcpyHostToDevice,
                                                   stream.get()));
                stream.synchronize();
                if (location_ == DataLocation::HOST) {
                    cudaFreeHost(data_);
                }
                data_ = device_data;
                capacity_ = padded_capacity;
            }
            location_ = location;
        }
    }

    DataLocation location() const { return location_; }

    const DType* data() const { return data_; }
    DType* data() { return data_; }

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
        // For CUDA, use padded capacity to support oblivious transfer
        if (location_ == DataLocation::CUDA) {
            new_capacity = nextPow2(new_capacity);
        }
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
                // Zero-initialize padding region to prevent data leaks
                CHECKED_CALL_THROW(cudaMemset(new_data + size_, 0, (new_capacity - size_) * sizeof(DType)));
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

    ConcreteVector slice(size_t start, size_t end) const
    {
        VELODB_ASSERT_MSG(start <= end && end <= size_, "Invalid slice range");
        VELODB_ASSERT_MSG(location_ == DataLocation::HOST, "Cannot slice non-host data");

        size_t new_size = end - start;
        return ConcreteVector(data_ + start, new_size, capacity_ - start, null_mask_.slice(start, end));
    }

    ConcreteVector tryOwn()
    {
        auto ret = ConcreteVector(data_, size_, capacity_, null_mask_);
        if (location_ != DataLocation::VIEW) {
            // Transfer ownership
            ret.location_ = location_;
            location_ = DataLocation::VIEW;
        }
        return ret;
    }

    ConcreteVector splitFront(size_t size)
    {
        VELODB_ASSERT_MSG(location_ != DataLocation::VIEW, "Cannot split a VIEW data source");

        // Create a new ValueVector for the second portion with correct capacity
        if (size > size_) {
            size = size_;
        }
        size_t remaining_size = size_ - size;
        ConcreteVector split_vector(/* capacity = */ size, location_);

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

    void appendMaskedMultiple(const ConcreteVector& other, const MaskVector& mask)
    {
        VELODB_ASSERT_MSG(location_ == DataLocation::CUDA && other.location_ == DataLocation::CUDA
                              && mask.location() == DataLocation::CUDA,
                          "Filter compaction must happen on CUDA");
        if (size_ + other.size_ > capacity_) {
            reserve(capacity_ + other.capacity_);
            null_mask_.reserve(null_mask_.element_capacity_ + other.null_mask_.element_capacity_);
        }
        auto stream_handle = StreamPool::instance().acquire().value_or_throw<ExecutionError>(
            "Failed to acquire stream for filter compaction");
        auto mask_elements = size_ / BitVector::ELEMENT_WIDTH;
        auto bit_offset = size_ % BitVector::ELEMENT_WIDTH;
        null_mask_.to(DataLocation::CUDA);
        auto num_added = filterCompact(data_ + size_,
                                       null_mask_.data_ + mask_elements,
                                       other.data_,
                                       other.null_mask_.data_,
                                       mask.data(),
                                       other.size_,
                                       bit_offset,
                                       stream_handle->get());
        null_mask_.to(DataLocation::HOST);
        size_ += num_added;
        null_mask_.size_ += num_added;
        stream_handle.release();
    }

    void appendMultiple(const ConcreteVector& other)
    {
        VELODB_ASSERT_MSG(location_ == other.location_ && location_ != DataLocation::VIEW,
                          fmt::format("Incompatible data locations: {} vs {}",
                                      location_, other.location_));
        if (size_ + other.size_ > capacity_) {
            reserve(DIV_UP(size_ + other.capacity_, capacity_) * capacity_);
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

    void reorder(const int64_t* indices)
    {
        auto stream_handle = StreamPool::instance().acquire().value_or_throw<ExecutionError>(
            "Failed to acquire stream for reorder");
        auto* new_data = reorderData(data_, indices, size_, stream_handle->get());
        std::swap(data_, new_data);
        if (new_data != nullptr) {
            cudaFree(new_data);
        }
        null_mask_.to(DataLocation::CUDA);
        auto* new_bitmap = reorderBitmap(null_mask_.data_, indices, size_, stream_handle->get());
        std::swap(null_mask_.data_, new_bitmap);
        null_mask_.to(DataLocation::HOST);
        if (new_bitmap != nullptr) {
            cudaFree(new_bitmap);
        }
        stream_handle->synchronize();
        stream_handle.release();
    }

protected:
    DType* data_;
    size_t size_;
    size_t capacity_;
    BitVector null_mask_;
    DataLocation location_;

    // Internal constructor
    ValueVectorBase(DType* data,
                    size_t size,
                    size_t capacity,
                    BitVector null_mask,
                    DataLocation location = DataLocation::VIEW)
        : data_(data)
        , size_(size)
        , capacity_(capacity)
        , null_mask_(std::move(null_mask))
        , location_(location)
    {
    }
};

template <typename VT>
class ValueVector : public ValueVectorBase<DTypeOfV<VT>, ValueVector> {
public:
    using Base = ValueVectorBase<DTypeOfV<VT>, ValueVector>;
    using DType = DTypeOfV<VT>;
    using VType = VT;
    using Base::Base;
    using typename Base::ConcreteVector;
    using typename Base::IntVector;
    using typename Base::MaskVector;

    Value get(size_t index) const
    {
        VELODB_ASSERT_MSG(location_ != DataLocation::CUDA, "Cannot access data on device");
        VELODB_ASSERT_MSG(index < size_, "Index out of range");
        if (null_mask_.get(index)) {
            return Value::createNull(dTypeId<DType>);
        }
        return Value(dTypeId<DType>, VType(data_[index]));
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
        vec.null_mask_.size_ = n;
        return vec;
    }

    static ValueVector buildFrom(const DType* data, size_t n, DataLocation location = DataLocation::HOST)
    {
        ValueVector vec(n, location);
        if (location == DataLocation::HOST) {
            std::copy(data, data + n, vec.data_);
        } else if (location == DataLocation::CUDA) {
            CHECKED_CALL_THROW(cudaMemcpy(vec.data_, data, n * sizeof(DType), cudaMemcpyDeviceToDevice));
        } else {
            VELODB_THROW(ExecutionError, "Invalid data location");
        }
        vec.size_ = n;
        vec.null_mask_.size_ = n;
        return vec;
    }

    static ValueVector materializeFrom(const ValueVector& source, const IntVector& rowids)
    {
        VELODB_ASSERT_MSG(source.location() == DataLocation::CUDA && rowids.location() == DataLocation::CUDA,
                          "Materialization must happen on CUDA");
        ValueVector vec(nextPow2(rowids.size()), DataLocation::CUDA);
        auto stream_handle = StreamPool::instance().acquire().value_or_throw<ExecutionError>(
            "Failed to acquire stream for filter compaction");
        materializeArray<DType>(vec.data_,
                                vec.null_mask_.data_,
                                source.data_,
                                source.null_mask_.data_,
                                rowids.data(),
                                rowids.size(),
                                stream_handle->get());
        stream_handle->synchronize();
        vec.size_ = rowids.size();
        vec.null_mask_.size_ = rowids.size();
        stream_handle.release();
        return vec;
    }

private:
    using Base::capacity_;
    using Base::data_;
    using Base::location_;
    using Base::null_mask_;
    using Base::size_;

    friend class ValueVectorBase<DTypeOfV<VT>, ValueVector>;
};

template <>
class ValueVector<OrdinalString> : public ValueVectorBase<size_t, ValueVector> {
public:
    using Base = ValueVectorBase<size_t, ValueVector>;
    using DType = size_t;
    using VType = OrdinalString;
    using Base::Base;
    using Base::ConcreteVector;
    using Base::MaskVector;

    ValueVector(size_t capacity, DataLocation location)
        : Base(capacity, location)
        , ordered_strings_(std::make_shared<std::vector<std::string>>())
    {
    }

    ValueVector(ValueVector&& other) noexcept
        : Base(std::move(other))
        , ordered_strings_(std::move(other.ordered_strings_))
    {
    }

    ValueVector& operator=(ValueVector&& other) noexcept
    {
        if (this != &other) {
            ordered_strings_ = std::move(other.ordered_strings_);
            Base::operator=(std::move(other));
        }
        return *this;
    }

    Value get(size_t index) const
    {
        VELODB_ASSERT_MSG(location_ != DataLocation::CUDA, "Cannot access data on device");
        VELODB_ASSERT_MSG(index < size_, "Index out of range");
        if (null_mask_.get(index)) {
            return Value::createNull(dTypeId<DType>);
        }
        auto ordinal = data_[index];
        return Value(dTypeId<DType>, VType(ordinal, std::string_view((*ordered_strings_)[ordinal])));
    }

    void ensureOrdinal(VType& data, ComparisonType comp) const
    {
        std::visit(
            [&data, comp, this](auto&& s) {
                size_t ordinal;
                // TODO: make this oblivious?
                switch (comp) {
                case ComparisonType::LESS_THAN:
                case ComparisonType::LESS_THAN_OR_EQUAL:
                case ComparisonType::GREATER_THAN:
                    ordinal = std::lower_bound(ordered_strings_->begin(), ordered_strings_->end(), s)
                        - ordered_strings_->begin();
                    break;
                case ComparisonType::GREATER_THAN_OR_EQUAL:
                    ordinal = std::upper_bound(ordered_strings_->begin(), ordered_strings_->end(), s)
                        - ordered_strings_->begin() - 1;
                    break;
                case ComparisonType::EQUAL:
                case ComparisonType::NOT_EQUAL: {
                    auto [lb, ub] = std::equal_range(ordered_strings_->begin(), ordered_strings_->end(), s);
                    if (lb == ub) {
                        ordinal = ordered_strings_->size();
                    } else {
                        ordinal = lb - ordered_strings_->begin();
                    }
                    break;
                }
                default:
                    VELODB_THROW(ExecutionError, "Unsupported comparison type");
                }
                data.ordinal_ = ordinal;
            },
            data.str_);
    }

    ConcreteVector slice(size_t start, size_t end) const
    {
        VELODB_ASSERT_MSG(start <= end && end <= size_, "Invalid slice range");
        VELODB_ASSERT_MSG(location_ == DataLocation::HOST, "Cannot slice non-host data");

        size_t new_size = end - start;
        return ConcreteVector(data_ + start, new_size, capacity_ - start, null_mask_.slice(start, end), ordered_strings_);
    }

    ConcreteVector tryOwn()
    {
        auto ret = ConcreteVector(data_, size_, capacity_, null_mask_, ordered_strings_);
        if (location_ != DataLocation::VIEW) {
            ret.location_ = location_;
            location_ = DataLocation::VIEW;
        }
        return ret;
    }

    ConcreteVector splitFront(size_t size)
    {
        auto split_vector = Base::splitFront(size);
        split_vector.ordered_strings_ = ordered_strings_;
        return split_vector;
    }

    void appendMaskedMultiple(const ConcreteVector& other, const MaskVector& mask)
    {
        Base::appendMaskedMultiple(other, mask);
        if (ordered_strings_ != other.ordered_strings_) {
            ordered_strings_ = other.ordered_strings_;
        }
    }

    void appendMultiple(const ConcreteVector& other)
    {
        Base::appendMultiple(other);
        if (ordered_strings_ != other.ordered_strings_) {
            ordered_strings_ = other.ordered_strings_;
        }
    }

    static ValueVector buildFrom(std::vector<Value>&& data, DataLocation location = DataLocation::HOST)
    {
        size_t n = data.size();
        ValueVector vec(n, location);

        // Build ordered string list
        for (const auto& value : data) {
            if (!value.isNull()) {
                vec.ordered_strings_->push_back(value.getString());
            }
        }
        std::sort(vec.ordered_strings_->begin(), vec.ordered_strings_->end());

        size_t i = 0;
        for (auto&& value : data) {
            if (value.isNull()) {
                vec.null_mask_.set(i);
            } else {
                // TODO: Make it oblivious?
                auto ordinal = std::distance(
                    vec.ordered_strings_->begin(),
                    std::lower_bound(vec.ordered_strings_->begin(), vec.ordered_strings_->end(), value.getString()));
                vec.data_[i] = static_cast<DType>(ordinal);
            }
            i++;
        }
        vec.size_ = n;
        vec.null_mask_.size_ = n;
        return vec;
    }

    static ValueVector materializeFrom(const ValueVector& source, const IntVector& rowids)
    {
        VELODB_ASSERT_MSG(source.location() == DataLocation::CUDA && rowids.location() == DataLocation::CUDA,
                          "Materialization must happen on CUDA");
        ValueVector vec(nextPow2(rowids.size()), DataLocation::CUDA);
        auto stream_handle = StreamPool::instance().acquire().value_or_throw<ExecutionError>(
            "Failed to acquire stream for filter compaction");
        materializeArray<DType>(vec.data_,
                                vec.null_mask_.data_,
                                source.data_,
                                source.null_mask_.data_,
                                rowids.data(),
                                rowids.size(),
                                stream_handle->get());
        stream_handle->synchronize();
        vec.size_ = rowids.size();
        vec.null_mask_.size_ = rowids.size();
        vec.ordered_strings_ = source.ordered_strings_;
        stream_handle.release();
        return vec;
    }

private:
    std::shared_ptr<std::vector<std::string>> ordered_strings_;

    // Internal constructor
    ValueVector(DType* data,
                size_t size,
                size_t capacity,
                BitVector null_mask,
                const std::shared_ptr<std::vector<std::string>>& ordered_strings)
        : Base(data, size, capacity, std::move(null_mask))
        , ordered_strings_(ordered_strings)
    {
    }
};

} // namespace velodb
