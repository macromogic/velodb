#include "catalog/column/column_info.hpp"

#include "common/fmt.hpp"

#include <fmt/format.h>

namespace velodb {

ColumnInfo::ColumnInfo(std::string name,
                       std::unique_ptr<DataType> type,
                       bool is_nullable,
                       bool is_unique,
                       bool is_primary_key)
    : name_(std::move(name))
    , type_(std::move(type))
    , is_nullable_(is_nullable)
    , is_unique_(is_unique)
    , is_primary_key_(is_primary_key)
{
}

ColumnInfo ColumnInfo::cloneImpl() const
{
    return { name_, type_->cloneUnique(), is_nullable_, is_unique_, is_primary_key_ };
}

std::string ColumnInfo::toString() const
{
    std::string result = fmt::format("{} {}", name_, *type_);
    if (!is_nullable_) {
        result += " NOT NULL";
    }
    if (is_unique_) {
        result += " UNIQUE";
    }
    if (is_primary_key_) {
        result += " PRIMARY KEY";
    }
    return result;
}

} // namespace velodb
