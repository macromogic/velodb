#pragma once

#include "common/copy_traits.hpp"
#include "common/exception.hpp"
#include "common/profiler.hpp"
#include "cuda/helper.hpp"
#include "cuda/host_memory_pool.hpp"
#include "cuda/pageable_memory_pool.hpp"
#include "cuda/staged_transfer.hpp"
#include "cuda/stream.hpp"
#include "cuda/stream_pool.hpp"
#include "data/bit_vector.hpp"
#include "data/data_location.hpp"
#include "data/type_traits.hpp"
#include "data/value.hpp"
#include "expression/expression.hpp"

#include <fmt/ranges.h>

#include <set>
#include <unordered_map>
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
        , null_mask_(capacity_, location)
        , location_(location)
    {
        if (location == DataLocation::HOST || location == DataLocation::HOST_PINNED) {
            data_ = static_cast<DType*>(HostMemoryPool::getInstance().allocate(capacity_ * sizeof(DType)));
            std::fill_n(data_, capacity_, DType());
            location_ = DataLocation::HOST_PINNED; // Normalize HOST to HOST_PINNED
        } else if (location == DataLocation::HOST_PAGEABLE) {
            data_ = static_cast<DType*>(PageableMemoryPool::getInstance().allocate(capacity_ * sizeof(DType)));
            std::fill_n(data_, capacity_, DType());
        } else if (location == DataLocation::CUDA) {
            auto stream_handle = StreamPool::getInstance().acquire().value();
            CHECKED_CALL_THROW(cudaMallocAsync(&data_, capacity_ * sizeof(DType), stream_handle->get()));
            CHECKED_CALL_THROW(cudaMemsetAsync(data_, 0, capacity_ * sizeof(DType), stream_handle->get()));
        } else {
            VELODB_THROW(ExecutionError, "Invalid data location");
        }
    }

    // Adoption constructor
    ValueVectorBase(DType* data, BitVector::Element* mask_data, size_t size, size_t capacity, DataLocation location)
        : data_(data)
        , size_(size)
        , capacity_(capacity)
        , null_mask_(mask_data, size, capacity, location)
        , location_(location)
    {
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
            if (!isViewLocation(location_) && data_ != nullptr) {
                if (location_ == DataLocation::CUDA) {
                    auto stream_handle = StreamPool::getInstance().acquire().value();
                    cudaFreeAsync(data_, stream_handle->get());
                } else if (location_ == DataLocation::HOST_PAGEABLE) {
                    PageableMemoryPool::getInstance().deallocate(data_, capacity_ * sizeof(DType));
                } else if (location_ == DataLocation::HOST_PINNED || location_ == DataLocation::HOST) {
                    HostMemoryPool::getInstance().deallocate(data_, capacity_ * sizeof(DType));
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
            other.location_ = DataLocation::HOST_PINNED;
        }
        return *this;
    }

    virtual ~ValueVectorBase()
    {
        if (data_ == nullptr) {
            return;
        }
        if (location_ == DataLocation::CUDA) {
            auto stream_handle = StreamPool::getInstance().acquire().value();
            cudaFreeAsync(data_, stream_handle->get());
            stream_handle->synchronize();
        } else if (location_ == DataLocation::HOST_PAGEABLE) {
            PageableMemoryPool::getInstance().deallocate(data_, capacity_ * sizeof(DType));
        } else if (location_ == DataLocation::HOST || location_ == DataLocation::HOST_PINNED) {
            HostMemoryPool::getInstance().deallocate(data_, capacity_ * sizeof(DType));
        }
        // VIEW and CUDA_VIEW don't own memory, nothing to free
    }

    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }

    void to(DataLocation location)
    {
        VELODB_ASSERT_MSG(location != DataLocation::VIEW && location != DataLocation::CUDA_VIEW,
                          "Cannot move data to VIEW or CUDA_VIEW");

        // Normalize target location
        DataLocation target = (location == DataLocation::HOST) ? DataLocation::HOST_PINNED : location;

        // Handle VIEW: need to copy data first to take ownership
        // Optimize: copy directly to target location instead of always going through HOST_PINNED
        if (location_ == DataLocation::VIEW) {
            // For VIEW, only transfer size_ elements (not capacity_)
            // capacity_ for VIEW represents max accessible range, not actual data size
            size_t transfer_size = size_;
            if (target == DataLocation::CUDA) {
                // VIEW -> CUDA: use pipelined staged transfer (VIEW data is in pageable memory)
                auto stream_handle = StreamPool::getInstance().acquire().value();
                size_t new_capacity = nextPow2(transfer_size);
                DType* device_data;
                CHECKED_CALL_THROW(cudaMallocAsync(&device_data, new_capacity * sizeof(DType), stream_handle->get()));
                stream_handle->synchronize();
                StagedTransfer::toDevicePipelined(device_data, data_, transfer_size * sizeof(DType));
                data_ = device_data;
                capacity_ = new_capacity;
                location_ = DataLocation::CUDA;
            } else if (target == DataLocation::HOST_PAGEABLE) {
                // VIEW -> HOST_PAGEABLE: copy to pageable memory
                DType* new_data = static_cast<DType*>(
                    PageableMemoryPool::getInstance().allocate(transfer_size * sizeof(DType)));
                std::copy(data_, data_ + transfer_size, new_data);
                data_ = new_data;
                capacity_ = transfer_size;
                location_ = DataLocation::HOST_PAGEABLE;
            } else {
                // VIEW -> HOST_PINNED: copy to pinned memory
                DType* new_data = static_cast<DType*>(
                    HostMemoryPool::getInstance().allocate(transfer_size * sizeof(DType)));
                std::copy(data_, data_ + transfer_size, new_data);
                data_ = new_data;
                capacity_ = transfer_size;
                location_ = DataLocation::HOST_PINNED;
            }
            // null_mask_ VIEW will be handled by its own to() call below
        } else if (location_ == DataLocation::CUDA_VIEW) {
            // Copy data to owned CUDA memory with proper padding for oblivious transfer
            // For VIEW, only transfer size_ elements
            auto stream_handle = StreamPool::getInstance().acquire().value();
            size_t padded_capacity = nextPow2(size_);
            DType* new_data;
            CHECKED_CALL_THROW(cudaMallocAsync(&new_data, padded_capacity * sizeof(DType), stream_handle->get()));
            CHECKED_CALL_THROW(cudaMemcpyAsync(new_data,
                                               data_,
                                               size_ * sizeof(DType),
                                               cudaMemcpyDeviceToDevice,
                                               stream_handle->get()));
            // Zero out padding region to prevent data leaks
            if (padded_capacity > size_) {
                CHECKED_CALL_THROW(cudaMemsetAsync(new_data + size_,
                                                   0,
                                                   (padded_capacity - size_) * sizeof(DType),
                                                   stream_handle->get()));
            }
            stream_handle->synchronize();
            data_ = new_data;
            capacity_ = padded_capacity;
            location_ = DataLocation::CUDA;
        }

        // Check if we've already reached target
        DataLocation current = (location_ == DataLocation::HOST) ? DataLocation::HOST_PINNED : location_;

        if (current == target) {
            null_mask_.to(location);
            return;
        }

        auto stream_handle = StreamPool::getInstance().acquire().value();

        if (current == DataLocation::CUDA) {
            // CUDA -> HOST_PINNED or CUDA -> HOST_PAGEABLE
            PROFILE_SCOPE("D2H Transfer: ValueVector");

            if (target == DataLocation::HOST_PINNED) {
                // CUDA -> HOST_PINNED: direct DMA with oblivious transfer (padded)
                size_t padded_capacity = nextPow2(capacity_);
                // Only use padded transfer if CUDA memory was allocated with padding
                size_t transfer_capacity = (capacity_ == padded_capacity) ? padded_capacity : capacity_;
                DType* host_data = static_cast<DType*>(
                    HostMemoryPool::getInstance().allocate(transfer_capacity * sizeof(DType)));
                CHECKED_CALL_THROW(cudaMemcpyAsync(host_data,
                                                   data_,
                                                   transfer_capacity * sizeof(DType),
                                                   cudaMemcpyDeviceToHost,
                                                   stream_handle->get()));
                CHECKED_CALL_THROW(cudaFreeAsync(data_, stream_handle->get()));
                stream_handle->synchronize();
                data_ = host_data;
                capacity_ = transfer_capacity;
            } else {
                // CUDA -> HOST_PAGEABLE: staged transfer (no padding needed for pageable)
                DType* pageable_data = static_cast<DType*>(
                    PageableMemoryPool::getInstance().allocate(capacity_ * sizeof(DType)));
                StagedTransfer::toHost(pageable_data, data_, capacity_ * sizeof(DType));
                CHECKED_CALL_THROW(cudaFreeAsync(data_, stream_handle->get()));
                stream_handle->synchronize();
                data_ = pageable_data;
            }
            location_ = target;
        } else if (target == DataLocation::CUDA) {
            // HOST_PINNED -> CUDA or HOST_PAGEABLE -> CUDA
            PROFILE_SCOPE("H2D Transfer: ValueVector");
            size_t padded_capacity = nextPow2(capacity_);
            DType* device_data;
            CHECKED_CALL_THROW(cudaMallocAsync(&device_data, padded_capacity * sizeof(DType), stream_handle->get()));

            if (current == DataLocation::HOST_PINNED) {
                // HOST_PINNED -> CUDA: direct DMA
                CHECKED_CALL_THROW(cudaMemcpyAsync(device_data,
                                                   data_,
                                                   capacity_ * sizeof(DType),
                                                   cudaMemcpyHostToDevice,
                                                   stream_handle->get()));
                HostMemoryPool::getInstance().deallocate(data_, capacity_ * sizeof(DType));
            } else {
                // HOST_PAGEABLE -> CUDA: use pipelined staged transfer for better throughput
                stream_handle->synchronize(); // Ensure device_data is allocated
                StagedTransfer::toDevicePipelined(device_data, data_, capacity_ * sizeof(DType));
                PageableMemoryPool::getInstance().deallocate(data_, capacity_ * sizeof(DType));
            }
            stream_handle->synchronize();
            data_ = device_data;
            capacity_ = padded_capacity;
            location_ = DataLocation::CUDA;
        } else {
            // HOST_PINNED <-> HOST_PAGEABLE
            PROFILE_SCOPE("ValueVector Host Memory Transfer");
            if (current == DataLocation::HOST_PINNED && target == DataLocation::HOST_PAGEABLE) {
                DType* pageable_data = static_cast<DType*>(
                    PageableMemoryPool::getInstance().allocate(capacity_ * sizeof(DType)));
                std::copy(data_, data_ + capacity_, pageable_data);
                HostMemoryPool::getInstance().deallocate(data_, capacity_ * sizeof(DType));
                data_ = pageable_data;
                location_ = DataLocation::HOST_PAGEABLE;
            } else {
                DType* pinned_data = static_cast<DType*>(
                    HostMemoryPool::getInstance().allocate(capacity_ * sizeof(DType)));
                std::copy(data_, data_ + capacity_, pinned_data);
                PageableMemoryPool::getInstance().deallocate(data_, capacity_ * sizeof(DType));
                data_ = pinned_data;
                location_ = DataLocation::HOST_PINNED;
            }
        }

        null_mask_.to(location);
    }

    DataLocation location() const { return location_; }

    const DType* data() const { return data_; }
    DType* data() { return data_; }

    void resize(size_t new_size, DType value = DType())
    {
        VELODB_ASSERT_MSG(isHostLocation(location_), "Cannot resize non-host data");
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
            auto stream_handle = StreamPool::getInstance().acquire().value();
            if (location_ == DataLocation::HOST || location_ == DataLocation::HOST_PINNED) {
                auto& host_memory_pool = HostMemoryPool::getInstance();
                DType* new_data = static_cast<DType*>(host_memory_pool.allocate(new_capacity * sizeof(DType)));
                stream_handle->synchronize();
                std::copy(data_, data_ + size_, new_data);
                host_memory_pool.deallocate(data_, capacity_ * sizeof(DType));
                data_ = new_data;
                capacity_ = new_capacity;
            } else if (location_ == DataLocation::HOST_PAGEABLE) {
                auto& pageable_memory_pool = PageableMemoryPool::getInstance();
                DType* new_data = static_cast<DType*>(pageable_memory_pool.allocate(new_capacity * sizeof(DType)));
                std::copy(data_, data_ + size_, new_data);
                pageable_memory_pool.deallocate(data_, capacity_ * sizeof(DType));
                data_ = new_data;
                capacity_ = new_capacity;
            } else if (location_ == DataLocation::CUDA) {
                DType* new_data;
                CHECKED_CALL_THROW(cudaMallocAsync(&new_data, new_capacity * sizeof(DType), stream_handle->get()));
                CHECKED_CALL_THROW(cudaMemcpyAsync(new_data,
                                                   data_,
                                                   size_ * sizeof(DType),
                                                   cudaMemcpyDeviceToDevice,
                                                   stream_handle->get()));
                // Zero-initialize padding region to prevent data leaks
                CHECKED_CALL_THROW(
                    cudaMemsetAsync(new_data + size_, 0, (new_capacity - size_) * sizeof(DType), stream_handle->get()));
                CHECKED_CALL_THROW(cudaFreeAsync(data_, stream_handle->get()));
                data_ = new_data;
                capacity_ = new_capacity;
            } else {
                VELODB_THROW(ExecutionError, "Cannot reserve data on VIEW");
            }
            stream_handle->synchronize();
        }
        null_mask_.reserve(new_capacity);
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

        size_t new_size = end - start;
        if (isCudaLocation(location_)) {
            // CUDA data: return a CUDA_VIEW
            return ConcreteVector(data_ + start,
                                  new_size,
                                  capacity_ - start,
                                  null_mask_.slice(start, end),
                                  DataLocation::CUDA_VIEW);
        } else {
            // HOST data: return a VIEW
            return ConcreteVector(data_ + start, new_size, capacity_ - start, null_mask_.slice(start, end));
        }
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
        VELODB_ASSERT_MSG(isHostLocation(location_) && isHostLocation(rowids.location()),
                          "Materialization gather must happen on HOST");
        // Use pageable memory for gather results to avoid exhausting the limited pinned memory pool.
        // Materialized results typically don't need DMA transfers.
        ConcreteVector vec(nextPow2(rowids.capacity()), DataLocation::HOST_PAGEABLE);
        const auto* rowid_data = rowids.data();
        for (size_t i = 0; i < rowids.capacity(); ++i) {
            auto rowid = static_cast<size_t>(rowid_data[i]) % size_;
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
            auto stream_handle = StreamPool::getInstance().acquire().value();
            DType value;
            CHECKED_CALL_THROW(
                cudaMemcpyAsync(&value, data_ + index, sizeof(DType), cudaMemcpyDeviceToHost, stream_handle->get()));
            stream_handle->synchronize();
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
        if (isHostLocation(location)) {
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
            auto stream_handle = StreamPool::getInstance().acquire().value();
            size_t ordinal;
            CHECKED_CALL_THROW(
                cudaMemcpyAsync(&ordinal, data_ + index, sizeof(size_t), cudaMemcpyDeviceToHost, stream_handle->get()));
            stream_handle->synchronize();
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

        size_t new_size = end - start;
        if (isCudaLocation(location_)) {
            // CUDA data: return a CUDA_VIEW
            return ConcreteVector(data_ + start,
                                  new_size,
                                  capacity_ - start,
                                  null_mask_.slice(start, end),
                                  ordered_strings_,
                                  DataLocation::CUDA_VIEW);
        } else {
            // HOST data: return a VIEW
            return ConcreteVector(data_ + start,
                                  new_size,
                                  capacity_ - start,
                                  null_mask_.slice(start, end),
                                  ordered_strings_);
        }
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

        // Use std::set for automatic deduplication and sorting
        std::set<std::string> unique_strings;
        for (const auto& value : data) {
            if (!value.isNull()) {
                unique_strings.insert(value.getString());
            }
        }

        // Initialize ordered_strings_ from sorted set and build ordinal map
        vec.ordered_strings_->reserve(unique_strings.size());
        std::unordered_map<std::string_view, size_t> string_to_ordinal;
        string_to_ordinal.reserve(unique_strings.size());

        size_t ord = 0;
        for (auto& str : unique_strings) {
            vec.ordered_strings_->push_back(std::move(str));
            string_to_ordinal[(*vec.ordered_strings_)[ord]] = ord;
            ord++;
        }

        // Assign ordinals using O(1) hash lookup
        size_t i = 0;
        for (auto&& value : data) {
            if (value.isNull()) {
                vec.null_mask_.set(i);
            } else {
                vec.data_[i] = static_cast<DType>(string_to_ordinal[value.getString()]);
            }
            i++;
        }
        vec.size_ = n;
        vec.null_mask_.size_ = n;
        return vec;
    }

    // Helper methods for StringColumnBuilder
    void setNull(size_t index) { null_mask_.set(index); }
    void setOrdinal(size_t index, size_t ordinal) { data_[index] = ordinal; }
    void setSize(size_t new_size)
    {
        size_ = new_size;
        null_mask_.size_ = new_size;
    }
    void setDictionary(size_t capacity) { ordered_strings_->reserve(capacity); }
    void setDictionaryEntry(size_t index, std::string&& str)
    {
        if (index >= ordered_strings_->size()) {
            ordered_strings_->resize(index + 1);
        }
        (*ordered_strings_)[index] = std::move(str);
    }

private:
    std::shared_ptr<std::vector<std::string>> ordered_strings_;

    // Internal constructor for slice (HOST VIEW)
    ValueVector(DType* data,
                size_t size,
                size_t capacity,
                BitVector null_mask,
                const std::shared_ptr<std::vector<std::string>>& ordered_strings)
        : Base(data, size, capacity, std::move(null_mask))
        , ordered_strings_(ordered_strings)
    {
    }

    // Internal constructor for sliceDevice (CUDA_VIEW)
    ValueVector(DType* data,
                size_t size,
                size_t capacity,
                BitVector null_mask,
                const std::shared_ptr<std::vector<std::string>>& ordered_strings,
                DataLocation location)
        : Base(data, size, capacity, std::move(null_mask), location)
        , ordered_strings_(ordered_strings)
    {
    }

    friend class Column;
};

} // namespace velodb
