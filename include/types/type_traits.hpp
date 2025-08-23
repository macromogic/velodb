#pragma once

#include "types/data_type.hpp"

namespace velodb {

template <DataTypeId T>
struct DataTypeTraits;

template <>
struct DataTypeTraits<DataTypeId::BOOLEAN> {
    using device_type = uint8_t;
};

template <>
struct DataTypeTraits<DataTypeId::TINYINT> {
    using device_type = int8_t;
};

template <>
struct DataTypeTraits<DataTypeId::SMALLINT> {
    using device_type = int16_t;
};

template <>
struct DataTypeTraits<DataTypeId::INTEGER> {
    using device_type = int32_t;
};

template <>
struct DataTypeTraits<DataTypeId::BIGINT> {
    using device_type = int64_t;
};

template <>
struct DataTypeTraits<DataTypeId::FLOAT> {
    using device_type = float;
};

template <>
struct DataTypeTraits<DataTypeId::DOUBLE> {
    using device_type = double;
};

} // namespace velodb
