#include "catalog/column/device_column.hpp"

#include "catalog/column/view_column.hpp"
#include "common/exception.hpp"
#include "common/fmt.hpp"

#include <fmt/format.h>

namespace velodb {

DeviceColumn::DeviceColumn(std::string name, DataType& type, size_t num_elements)
    : ColumnBase(name, /*is_nullable = */ false, /*is_unique = */ false, /*is_primary_key = */ false)
    , type_(type)
    , num_elements_(num_elements)
{
    cudaMalloc(&device_data_, num_elements_ * type_.size());
    cudaStreamCreate(&stream_); // TODO: use stream pool
}

size_t DeviceColumn::size() const
{
    return num_elements_;
}

const Value& DeviceColumn::get([[maybe_unused]] size_t row) const
{
    VELODB_THROW(CatalogError, "Cannot access device data");
}

ViewColumn DeviceColumn::view() const
{
    return ViewColumn(type_, getName(), *this);
}

ViewColumn DeviceColumn::viewAs(std::string alias) const
{
    return ViewColumn(type_, std::move(alias), *this);
}

void DeviceColumn::setStream(cudaStream_t stream)
{
    stream_ = stream;
}

std::string DeviceColumn::toString() const
{
    std::string result = fmt::format("DeviceColumn(name={}, type={}, size={}", getName(), type_, size());
    if (!isNullable()) {
        result += ", not null";
    }
    if (isUnique()) {
        result += ", unique";
    }
    if (isPrimaryKey()) {
        result += ", primary key";
    }
    result += ")";
    return result;
}

} // namespace velodb
