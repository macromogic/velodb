#include "data/value_vector.hpp"

#include "common/exception.hpp"
#include "cuda/stream.hpp"
#include "data/value.hpp"

#include <algorithm>

namespace velodb {

// ValueVector<size_t> specialization implementation (StringVector)

ValueVector<size_t>::ValueVector(size_t capacity, DataLocation location)
    : data_(nullptr)
    , size_(0)
    , capacity_(capacity)
    , null_mask_(capacity)
    , location_(location)
    , ordered_strings_(std::make_shared<std::vector<std::string>>())
{
    if (location == DataLocation::HOST) {
        CHECKED_CALL_THROW(cudaMallocHost(&data_, capacity_ * sizeof(DType)));
    } else if (location == DataLocation::CUDA) {
        CHECKED_CALL_THROW(cudaMalloc(&data_, capacity_ * sizeof(DType)));
    } else {
        VELODB_THROW(ExecutionError, "Invalid data location");
    }
}

ValueVector<size_t>::ValueVector(ValueVector&& other) noexcept
    : data_(other.data_)
    , size_(other.size_)
    , capacity_(other.capacity_)
    , null_mask_(std::move(other.null_mask_))
    , location_(other.location_)
    , ordered_strings_(std::move(other.ordered_strings_))
{
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
    other.location_ = DataLocation::HOST;
}

ValueVector<size_t>& ValueVector<size_t>::operator=(ValueVector&& other) noexcept
{
    if (this != &other) {
        if (location_ != DataLocation::VIEW && data_ != nullptr) {
            if (location_ == DataLocation::HOST) {
                cudaFreeHost(data_);
            } else {
                cudaFree(data_);
            }
        }

        data_ = other.data_;
        size_ = other.size_;
        capacity_ = other.capacity_;
        null_mask_ = std::move(other.null_mask_);
        location_ = other.location_;
        ordered_strings_ = std::move(other.ordered_strings_);

        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
        other.location_ = DataLocation::HOST;
    }
    return *this;
}

ValueVector<size_t>::~ValueVector()
{
    if (location_ == DataLocation::VIEW) {
        return;
    }
    if (data_ != nullptr) {
        if (location_ == DataLocation::HOST) {
            cudaFreeHost(data_);
        } else {
            cudaFree(data_);
        }
    }
}

size_t ValueVector<size_t>::size() const
{
    return size_;
}

Value ValueVector<size_t>::get(size_t index) const
{
    VELODB_ASSERT_MSG(location_ != DataLocation::CUDA, "Cannot access data on device");
    VELODB_ASSERT_MSG(index < size_, "Index out of range");
    if (null_mask_.get(index)) {
        return Value::createNull(dTypeId<DType>);
    }
    auto ordinal = data_[index];
    return Value(dTypeId<DType>, VType(ordinal, std::string_view((*ordered_strings_)[ordinal])));
}

const ValueVector<size_t>::DType* ValueVector<size_t>::data() const
{
    return data_;
}

void ValueVector<size_t>::ensureOrdinal(VType& data, ComparisonType comp) const
{
    std::visit(
        [&data, comp, this](auto&& arg) {
            size_t ordinal;
            // TODO: make this oblivious?
            switch (comp) {
            case ComparisonType::LESS_THAN:
            case ComparisonType::LESS_THAN_OR_EQUAL:
            case ComparisonType::GREATER_THAN:
                ordinal = std::lower_bound(ordered_strings_->begin(), ordered_strings_->end(), arg)
                    - ordered_strings_->begin();
                break;
            case ComparisonType::GREATER_THAN_OR_EQUAL:
                ordinal = std::upper_bound(ordered_strings_->begin(), ordered_strings_->end(), arg)
                    - ordered_strings_->begin() - 1;
                break;
            case ComparisonType::EQUAL:
            case ComparisonType::NOT_EQUAL: {
                auto [lb, ub] = std::equal_range(ordered_strings_->begin(), ordered_strings_->end(), arg);
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

Result<EventPool::EventHandle> ValueVector<size_t>::to(DataLocation location)
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
            CHECKED_CALL_THROW(
                cudaMemcpyAsync(device_data, data_, capacity_ * sizeof(DType), cudaMemcpyHostToDevice, stream.get()));
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

DataLocation ValueVector<size_t>::location() const
{
    return location_;
}

void ValueVector<size_t>::resize(size_t new_size, DType value)
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

void ValueVector<size_t>::reserve(size_t new_capacity)
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

void ValueVector<size_t>::append(const DType& value)
{
    if (size_ >= capacity_) {
        reserve(capacity_ * 2);
    }
    null_mask_.resize(size_ + 1);
    data_[size_] = value;
    size_++;
}

void ValueVector<size_t>::append(DType&& value)
{
    if (size_ >= capacity_) {
        reserve(capacity_ * 2);
    }
    null_mask_.resize(size_ + 1);
    data_[size_] = std::move(value);
    size_++;
}

ValueVector<size_t> ValueVector<size_t>::slice(size_t start, size_t end) const
{
    VELODB_ASSERT_MSG(start <= end && end <= size_, "Invalid slice range");
    VELODB_ASSERT_MSG(location_ == DataLocation::HOST, "Cannot slice non-host data");

    size_t new_size = end - start;
    return ValueVector(data_ + start, new_size, capacity_, null_mask_.slice(start, end), ordered_strings_);
}

ValueVector<size_t> ValueVector<size_t>::tryOwn()
{
    auto ret = ValueVector(data_, size_, capacity_, null_mask_, ordered_strings_);
    if (location_ != DataLocation::VIEW) {
        ret.location_ = location_;
        location_ = DataLocation::VIEW;
    }
    return ret;
}

ValueVector<size_t> ValueVector<size_t>::splitFront(size_t size)
{
    VELODB_ASSERT_MSG(location_ != DataLocation::VIEW, "Cannot split a VIEW data source");

    if (size > size_) {
        size = size_;
    }
    size_t remaining_size = size_ - size;
    ValueVector<size_t> split_vector(/* capacity = */ size, location_);

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
    split_vector.ordered_strings_ = ordered_strings_;

    null_mask_ = null_mask_.slice(size, size_);
    size_ = remaining_size;

    return split_vector;
}

void ValueVector<size_t>::appendMultiple(const ValueVector<size_t>& other, const ValueVector<uint8_t>& mask)
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
    if (ordered_strings_ != other.ordered_strings_) {
        ordered_strings_ = other.ordered_strings_;
    }
    stream_handle.release();
}

ValueVector<size_t> ValueVector<size_t>::buildFrom(std::vector<Value>&& data, DataLocation location)
{
    size_t n = data.size();
    ValueVector<size_t> vec(n, location);

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
    return vec;
}

// Private constructor implementation
ValueVector<size_t>::ValueVector(DType* data,
                                 size_t size,
                                 size_t capacity,
                                 BitVector null_mask,
                                 const std::shared_ptr<std::vector<std::string>>& ordered_strings)
    : data_(data)
    , size_(size)
    , capacity_(capacity)
    , null_mask_(std::move(null_mask))
    , location_(DataLocation::VIEW)
    , ordered_strings_(ordered_strings)
{
}

} // namespace velodb
