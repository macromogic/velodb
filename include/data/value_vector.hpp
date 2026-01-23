#pragma once

#include "common/copy_traits.hpp"
#include "common/exception.hpp"
#include "common/profiler.hpp"
#include "cuda/allocator.hpp"
#include "cuda/helper.hpp"
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
        , null_mask_(0, location)
        , location_(location)
    {
        null_mask_.reserve(capacity_);
        if (location == DataLocation::HOST) {
            data_ = MemoryAllocator::allocate<DType>(DataLocation::HOST, capacity_);
        } else if (location == DataLocation::CUDA) {
            data_ = MemoryAllocator::allocate<DType>(DataLocation::CUDA, capacity_);
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
                MemoryAllocator::deallocate(data_);
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
            MemoryAllocator::deallocate(data_);
        }
    }

    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }

    void to(DataLocation location)
    {
        VELODB_ASSERT_MSG(location != DataLocation::VIEW, "Cannot move data to VIEW");
        if (location_ != location) {
            auto stream_handler = StreamPool::getInstance().acquire().value();
            if (location_ == DataLocation::CUDA) {
                PROFILE_SCOPE("ValueVector D2H Transfer");
                // Oblivious transfer: round up to next power of 2 to hide selectivity
                size_t padded_capacity = nextPow2(capacity_);
                VELODB_ASSERT_MSG(capacity_ >= padded_capacity, "Insufficient capacity for oblivious transfer");
                DType* host_data = MemoryAllocator::allocate<DType>(DataLocation::HOST,
                                                                    padded_capacity,
                                                                    stream_handler->get());
                CHECKED_CALL_THROW(cudaMemcpyAsync(host_data,
                                                   data_,
                                                   padded_capacity * sizeof(DType),
                                                   cudaMemcpyDeviceToHost,
                                                   stream_handler->get()));
                MemoryAllocator::deallocate(data_, stream_handler->get());
                stream_handler->synchronize();
                data_ = host_data;
                capacity_ = padded_capacity;
            } else {
                PROFILE_SCOPE("ValueVector H2D Transfer");
                size_t padded_capacity = nextPow2(capacity_);
                DType* device_data = MemoryAllocator::allocate<DType>(DataLocation::CUDA,
                                                                      padded_capacity,
                                                                      stream_handler->get());
                CHECKED_CALL_THROW(cudaMemcpyAsync(device_data,
                                                   data_,
                                                   capacity_ * sizeof(DType),
                                                   cudaMemcpyHostToDevice,
                                                   stream_handler->get()));
                if (location_ == DataLocation::HOST) {
                    MemoryAllocator::deallocate(data_, stream_handler->get());
                }
                stream_handler->synchronize();
                data_ = device_data;
                capacity_ = padded_capacity;
            }
            location_ = location;
        }
        null_mask_.to(location);
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
            auto stream_handler = StreamPool::getInstance().acquire().value();
            if (location_ == DataLocation::HOST) {
                DType* new_data = MemoryAllocator::allocate<DType>(DataLocation::HOST,
                                                                   new_capacity,
                                                                   stream_handler->get());
                stream_handler->synchronize();
                std::copy(data_, data_ + size_, new_data);
                MemoryAllocator::deallocate(data_, stream_handler->get());
                data_ = new_data;
                capacity_ = new_capacity;
            } else if (location_ == DataLocation::CUDA) {
                DType* new_data = MemoryAllocator::allocate<DType>(DataLocation::CUDA,
                                                                   new_capacity,
                                                                   stream_handler->get());
                CHECKED_CALL_THROW(cudaMemcpyAsync(new_data,
                                                   data_,
                                                   size_ * sizeof(DType),
                                                   cudaMemcpyDeviceToDevice,
                                                   stream_handler->get()));
                // Zero-initialize padding region to prevent data leaks
                CHECKED_CALL_THROW(cudaMemsetAsync(new_data + size_,
                                                   0,
                                                   (new_capacity - size_) * sizeof(DType),
                                                   stream_handler->get()));
                MemoryAllocator::deallocate(data_, stream_handler->get());
                data_ = new_data;
                capacity_ = new_capacity;
            } else {
                VELODB_THROW(ExecutionError, "Cannot reserve data on VIEW");
            }
            stream_handler->synchronize();
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
        auto ret = ConcreteVector(data_, size_, capacity_, null_mask_.clone());
        if (location_ != DataLocation::VIEW) {
            // Transfer ownership
            ret.location_ = location_;
            location_ = DataLocation::VIEW;
        }
        return ret;
    }

    ConcreteVector gather(const IntVector& rowids) const
    {
        VELODB_ASSERT_MSG(location_ == DataLocation::HOST && rowids.location() == DataLocation::HOST,
                          "Materialization gather must happen on HOST");
        ConcreteVector vec(nextPow2(rowids.capacity()), DataLocation::HOST);
        const auto* rowid_data = rowids.data();
        for (size_t i = 0; i < rowids.capacity(); ++i) {
            auto rowid = static_cast<size_t>(rowid_data[i]);
            VELODB_ASSERT_MSG(rowid < size_, "Rowid out of range");
            if (null_mask_.get(rowid)) {
                vec.null_mask_.set(i);
            } else {
                vec.data_[i] = data_[rowid];
            }
        }
        vec.size_ = rowids.size();
        vec.null_mask_.size_ = rowids.size();
        return vec;
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
                    BitVector&& null_mask,
                    DataLocation location = DataLocation::VIEW)
        : data_(data)
        , size_(size)
        , capacity_(capacity)
        , null_mask_(std::move(null_mask))
        , location_(location)
    {
    }

    void setSize(size_t new_size)
    {
        size_ = new_size;
        null_mask_.size_ = new_size;
    }
    friend class Column;
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
        VELODB_ASSERT_MSG(index < size_, "Index out of range");
        if (null_mask_.get(index)) {
            return Value::createNull(dTypeId<DType>);
        }
        if (location_ == DataLocation::CUDA) {
            auto stream_handler = StreamPool::getInstance().acquire().value();
            DType value;
            CHECKED_CALL_THROW(
                cudaMemcpyAsync(&value, data_ + index, sizeof(DType), cudaMemcpyDeviceToHost, stream_handler->get()));
            stream_handler->synchronize();
            return Value(dTypeId<DType>, VType(value));
        } else {
            return Value(dTypeId<DType>, VType(data_[index]));
        }
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

private:
    using Base::capacity_;
    using Base::data_;
    using Base::location_;
    using Base::null_mask_;
    using Base::size_;

    friend class ValueVectorBase<DTypeOfV<VT>, ValueVector>;
    friend class Column;
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
        VELODB_ASSERT_MSG(index < size_, "Index out of range");
        if (null_mask_.get(index)) {
            return Value::createNull(dTypeId<DType>);
        }
        if (location_ == DataLocation::CUDA) {
            auto stream_handler = StreamPool::getInstance().acquire().value();
            size_t ordinal;
            CHECKED_CALL_THROW(cudaMemcpyAsync(&ordinal,
                                               data_ + index,
                                               sizeof(size_t),
                                               cudaMemcpyDeviceToHost,
                                               stream_handler->get()));
            stream_handler->synchronize();
            return Value(dTypeId<DType>, VType(ordinal, std::string_view((*ordered_strings_)[ordinal])));
        } else {
            auto ordinal = data_[index];
            return Value(dTypeId<DType>, VType(ordinal, std::string_view((*ordered_strings_)[ordinal])));
        }
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
        return ConcreteVector(data_ + start,
                              new_size,
                              capacity_ - start,
                              null_mask_.slice(start, end),
                              ordered_strings_);
    }

    ConcreteVector tryOwn()
    {
        auto ret = ConcreteVector(data_, size_, capacity_, null_mask_.clone(), ordered_strings_);
        if (location_ != DataLocation::VIEW) {
            ret.location_ = location_;
            location_ = DataLocation::VIEW;
        }
        return ret;
    }

    ConcreteVector gather(const IntVector& rowids) const
    {
        auto vec = Base::gather(rowids);
        vec.ordered_strings_ = ordered_strings_;
        return vec;
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

    friend class Column;
};

} // namespace velodb
