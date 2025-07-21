#include "types/data_type.hpp"
#include "common/traced_exception.hpp"
#include <stdexcept>

namespace velodb {

DataType::DataType(DataTypeId type_id, size_t size)
    : type_id_(type_id)
    , size_(size)
{
}

std::unique_ptr<DataType> DataType::createType(DataTypeId type_id, size_t size)
{
    switch (type_id) {
    case DataTypeId::BOOLEAN:
        return std::make_unique<BooleanType>();
    case DataTypeId::INTEGER:
        return std::make_unique<IntegerType>();
    case DataTypeId::BIGINT:
        return std::make_unique<BigIntType>();
    case DataTypeId::DOUBLE:
        return std::make_unique<DoubleType>();
    case DataTypeId::VARCHAR:
        return std::make_unique<VarcharType>(size);
    default:
        VELODB_THROW(TypeError, "Unsupported data type");
    }
}

} // namespace velodb
