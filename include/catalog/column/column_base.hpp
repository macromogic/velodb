#pragma once

#include "common/copy_traits.hpp"
#include "types/data_type.hpp"

#include <string>

namespace velodb {

// Forward declarations
class ViewColumn;
class Value;

class ColumnBase : private NonCopyable {
public:
    virtual size_t size() const = 0;
    virtual const Value& get(size_t row) const = 0;

    virtual DataType& getType() const = 0;
    std::string getName() const { return name_; }
    bool isNullable() const { return is_nullable_; }
    bool isUnique() const { return is_unique_; }
    bool isPrimaryKey() const { return is_primary_key_; }

    virtual ViewColumn view() const = 0;
    virtual ViewColumn viewAs(std::string alias) const = 0;

    virtual std::string toString() const = 0;

protected:
    ColumnBase(std::string name, bool is_nullable = true, bool is_unique = false, bool is_primary_key = false)
        : name_(std::move(name))
        , is_nullable_(is_nullable)
        , is_unique_(is_unique)
        , is_primary_key_(is_primary_key)
    {
    }

private:
    std::string name_;
    bool is_nullable_;
    bool is_unique_;
    bool is_primary_key_;
};

} // namespace velodb
