#pragma once

#include "catalog/column/column_base.hpp"
#include "types/type_traits.hpp"
#include "types/value.hpp"

#include <cuda_runtime.h>

namespace velodb {

class ViewColumn; // Forward declaration

class DeviceColumn : public ColumnBase {
public:
    DeviceColumn(std::string name, DataType& type, size_t num_elements);

    size_t size() const override;
    const Value& get(size_t row) const override;
    DataType& getType() const override { return type_; }

    ViewColumn view() const override;
    ViewColumn viewAs(std::string alias) const override;

    void setStream(cudaStream_t stream);

    std::string toString() const override;

    template <typename T>
    T* data()
    {
        return reinterpret_cast<T*>(device_data_);
    }

    template <typename T>
    const T* data() const
    {
        return reinterpret_cast<const T*>(device_data_);
    }

private:
    DataType& type_;
    void* device_data_;
    size_t num_elements_;
    cudaStream_t stream_;

    // Buffers for host accesses
};

} // namespace velodb
