#pragma once

#include "common/non_copyable.hpp"
#include "types/value.hpp"

namespace velodb {

// TODO: test if it can be non-copyable
class ColumnInfo : private NonCopyable {
public:
    ColumnInfo(std::string name,
        std::unique_ptr<DataType> type,
        bool is_nullable = true,
        bool is_unique = false,
        bool is_primary_key = false)
        : name_(std::move(name))
        , type_(std::move(type))
        , is_nullable_(is_nullable)
        , is_unique_(is_unique)
        , is_primary_key_(is_primary_key)
    {
    }

    const std::string& getName() const { return name_; }
    const DataType& getType() const { return *type_; }
    bool isNullable() const { return is_nullable_; }
    bool isUnique() const { return is_unique_; }
    bool isPrimaryKey() const { return is_primary_key_; }

    ColumnInfo clone() const
    {
        return ColumnInfo(name_, type_->clone(), is_nullable_, is_unique_, is_primary_key_);
    }

    std::string toString() const
    {
        return name_ + " " + type_->toString();
    }

private:
    std::string name_;
    std::unique_ptr<DataType> type_;
    bool is_nullable_;
    bool is_unique_;
    bool is_primary_key_;
};

class ViewColumn; // Forward declaration

class Column : private NonCopyable {
public:
    virtual size_t size() const = 0;
    virtual Value get(size_t row) const = 0;
    virtual Value operator[](size_t row) const
    {
        return get(row);
    }

    virtual DataType& getType() const = 0;
    std::string getName() const { return name_; }
    bool isNullable() const { return is_nullable_; }
    bool isUnique() const { return is_unique_; }
    bool isPrimaryKey() const { return is_primary_key_; }

    virtual std::string toString() const = 0;

protected:
    Column(std::string name,
        bool is_nullable = true,
        bool is_unique = false,
        bool is_primary_key = false)
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

class ValueColumn : public Column {
public:
    ValueColumn(std::string name,
        std::unique_ptr<DataType> type,
        bool is_nullable = true,
        bool is_unique = false,
        bool is_primary_key = false)
        : Column(std::move(name), is_nullable, is_unique, is_primary_key)
        , type_(std::move(type))
    {
    }

    explicit ValueColumn(const ColumnInfo& info)
        : Column(info.getName(), info.isNullable(), info.isUnique(), info.isPrimaryKey())
        , type_(info.getType().clone())
    {
    }

    void resize(size_t new_size);
    void reserve(size_t new_capacity);
    size_t size() const override;
    Value get(size_t row) const override;
    void append(const Value& value);

    DataType& getType() const override { return *type_; }

    ViewColumn view() const;
    ViewColumn viewAs(std::string alias) const;

    std::string toString() const override;

private:
    std::unique_ptr<DataType> type_;
    ValueVector values_; // Stores column values
};

class ViewColumn : public Column {
public:
    ViewColumn(DataType& type, std::string name, const ValueVector& values)
        : Column(std::move(name), true, false, false)
        , type_(type)
        , values_(values)
    {
    }

    size_t size() const override;
    Value get(size_t row) const override;

    DataType& getType() const override { return type_; }
    const ValueVector& getValues() const { return values_; }

    std::string toString() const override;

private:
    DataType& type_;
    const ValueVector& values_; // Reference to the column values in the view
};

} // namespace velodb