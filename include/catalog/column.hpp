#pragma once

#include "common/copy_traits.hpp"
#include "types/value.hpp"

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

class ViewColumn; // Forward declaration

class Column : private NonCopyable {
public:
    virtual size_t size() const = 0;
    virtual Value get(size_t row) const = 0;

    virtual DataType& getType() const = 0;
    std::string getName() const { return name_; }
    bool isNullable() const { return is_nullable_; }
    bool isUnique() const { return is_unique_; }
    bool isPrimaryKey() const { return is_primary_key_; }

    virtual ViewColumn view() const = 0;
    virtual ViewColumn viewAs(std::string alias) const = 0;

    virtual std::string toString() const = 0;

protected:
    Column(std::string name,
        bool is_nullable = true,
        bool is_unique = false,
        bool is_primary_key = false);

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
        bool is_primary_key = false);

    explicit ValueColumn(const ColumnInfo& info);

    void resize(size_t new_size);
    void reserve(size_t new_capacity);
    size_t size() const override;
    Value get(size_t row) const override;
    Value& operator[](size_t row);
    void append(const Value& value);
    void fill(const Value& value, size_t count);

    DataType& getType() const override { return *type_; }

    ViewColumn view() const override;
    ViewColumn viewAs(std::string alias) const override;

    std::string toString() const override;

private:
    std::unique_ptr<DataType> type_;
    ValueVector values_; // Stores column values
};

class ViewColumn : public Column {
public:
    ViewColumn(DataType& type, std::string name, const ValueVector& values);

    size_t size() const override;
    Value get(size_t row) const override;

    DataType& getType() const override { return type_; }
    const ValueVector& getValues() const { return values_; }

    ViewColumn view() const override;
    ViewColumn viewAs(std::string alias) const override;

    std::string toString() const override;

private:
    DataType& type_;
    const ValueVector& values_; // Reference to the column values in the view
};

} // namespace velodb
