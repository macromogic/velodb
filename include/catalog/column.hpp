#pragma once

#include "common/copy_traits.hpp"
#include "data/data_type.hpp"
#include "data/value_vector.hpp"
#include "expression/expression.hpp"

#include <memory>
#include <string>

namespace velodb {

// Forward declaration
class RowBatch;

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

class Column : private NonCopyable {
public:
    using DataSource = std::variant<ValueVector<bool>,
                                    ValueVector<int8_t>,
                                    ValueVector<int16_t>,
                                    ValueVector<int32_t>,
                                    ValueVector<int64_t>,
                                    ValueVector<float>,
                                    ValueVector<double>,
                                    ValueVector<OrdinalString>>;

    explicit Column(std::unique_ptr<DataType> type,
                    size_t initial_capacity = 16,
                    DataLocation location = DataLocation::HOST);
    Column(Column&& other) = default;
    Column& operator=(Column&& other) = default;
    ~Column() = default;

    static Column buildFrom(std::unique_ptr<DataType> type,
                            std::vector<Value>&& values,
                            DataLocation location = DataLocation::HOST);

    const DataType& getType() const;
    size_t size() const;
    Value get(size_t index) const;
    Value operator[](size_t index) const;
    void ensureOrdinal(Value& value, ComparisonType comp) const;

    DataLocation location() const;
    void to(DataLocation location);

    void reserve(size_t new_capacity);
    void append(const Value& value);
    void append(Value&& value);

    Column slice(size_t begin, size_t end) const;
    Column tryOwn();
    Column splitFront(size_t size);
    void reorder(const int64_t* indices);

private:
    std::unique_ptr<DataType> type_;
    DataSource data_source_;

    Column(std::unique_ptr<DataType> type, DataSource data_source)
        : type_(std::move(type))
        , data_source_(std::move(data_source))
    {
    }

    void appendMaskedMultiple(const Column& other, const Column& mask);
    void appendMultiple(const Column& other);

    friend class RowBatch;
};

} // namespace velodb
