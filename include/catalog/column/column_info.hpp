#pragma once

#include "common/copy_traits.hpp"
#include "data/data_type.hpp"

#include <memory>
#include <string>

namespace velodb {

class ColumnInfo : private NonCopyable, public Cloneable<ColumnInfo> {
public:
    ColumnInfo(std::string name,
               std::unique_ptr<DataType> type,
               bool is_nullable = true,
               bool is_unique = false,
               bool is_primary_key = false);

    const std::string& getName() const { return name_; }
    const DataType& getType() const { return *type_; }
    bool isNullable() const { return is_nullable_; }
    bool isUnique() const { return is_unique_; }
    bool isPrimaryKey() const { return is_primary_key_; }

    std::string toString() const;

private:
    std::string name_;
    std::unique_ptr<DataType> type_;
    bool is_nullable_;
    bool is_unique_;
    bool is_primary_key_;

    friend class Cloneable<ColumnInfo>;
    ColumnInfo cloneImpl() const;
};

} // namespace velodb
