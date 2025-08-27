#pragma once

#include "data/data_type.hpp"
#include "data/fixed_string.hpp"

namespace velodb {

#define LIST_DTYPES(X)                                                                                                 \
    X(BOOLEAN, uint8_t)                                                                                                \
    X(TINYINT, int8_t)                                                                                                 \
    X(SMALLINT, int16_t)                                                                                               \
    X(INTEGER, int32_t)                                                                                                \
    X(BIGINT, int64_t)                                                                                                 \
    X(FLOAT, float)                                                                                                    \
    X(DOUBLE, double)                                                                                                  \
    X(VARCHAR, FixedString)

#define LIST_DVTYPES(X)                                                                                                \
    X(BOOLEAN, uint8_t, bool)                                                                                          \
    X(TINYINT, int8_t, int8_t)                                                                                         \
    X(SMALLINT, int16_t, int16_t)                                                                                      \
    X(INTEGER, int32_t, int32_t)                                                                                       \
    X(BIGINT, int64_t, int64_t)                                                                                        \
    X(FLOAT, float, float)                                                                                             \
    X(DOUBLE, double, double)                                                                                          \
    X(VARCHAR, FixedString, FixedString)

// Trait template for data type id

template <DataTypeId Id>
struct TypeIdTraits;
#define X(name, DT, VT)                                                                                                \
    template <>                                                                                                        \
    struct TypeIdTraits<DataTypeId::name> {                                                                            \
        using DeviceType = DT;                                                                                         \
        using ValueType = VT;                                                                                          \
    };
LIST_DVTYPES(X)
#undef X

template <DataTypeId Id>
using DTypeOf = typename TypeIdTraits<Id>::DeviceType;

template <DataTypeId Id>
using VTypeOf = typename TypeIdTraits<Id>::ValueType;

// Trait template for data type (for storage)

template <typename DT>
struct DataTypeTraits;
#define X(name, DT, VT)                                                                                                \
    template <>                                                                                                        \
    struct DataTypeTraits<DT> {                                                                                        \
        static inline constexpr DataTypeId id = DataTypeId::name;                                                      \
        using ValueType = VT;                                                                                          \
    };
LIST_DVTYPES(X)
#undef X

template <typename DT>
inline constexpr DataTypeId dTypeId = DataTypeTraits<DT>::id;

template <typename DT>
using VTypeOfD = typename DataTypeTraits<DT>::ValueType;

// Trait template for data type (for access)

template <typename VT>
struct ValueTypeTraits;
#define X(name, DT, VT)                                                                                                \
    template <>                                                                                                        \
    struct ValueTypeTraits<VT> {                                                                                       \
        static inline constexpr DataTypeId id = DataTypeId::name;                                                      \
        using DeviceType = DT;                                                                                         \
    };
LIST_DVTYPES(X)
#undef X

template <typename VT>
inline constexpr DataTypeId vTypeId = ValueTypeTraits<VT>::id;

template <typename VT>
using DTypeOfV = typename ValueTypeTraits<VT>::DeviceType;

} // namespace velodb
