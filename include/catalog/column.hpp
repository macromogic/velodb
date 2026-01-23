#pragma once

#include "common/copy_traits.hpp"
#include "data/data_type.hpp"
#include "data/value_vector.hpp"
#include "expression/expression.hpp"

#include <memory>
#include <string>

namespace velodb {

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
                                    ValueVector<OrdinalString>,
                                    ValueVector<uint32_t>>;

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

    void* rawData();
    const void* rawData() const;
    BitVector::Element* rawBitmapData();
    const BitVector::Element* rawBitmapData() const;
    void* getDeviceBuffer() const;
    BitVector::Element* getDeviceBitmapBuffer() const;
    void setFromDeviceBuffers(void* data, BitVector::Element* bitmap_data);

    DataLocation location() const;
    void to(DataLocation location);

    void reserve(size_t new_capacity);
    void append(const Value& value);
    void append(Value&& value);

    Column slice(size_t begin, size_t end) const;
    Column gather(const Column& rowids) const;
    Column tryOwn();

    void debug() const;
    void debug(size_t max_elements) const;

private:
    std::unique_ptr<DataType> type_;
    DataSource data_source_;

    Column(std::unique_ptr<DataType> type, DataSource data_source)
        : type_(std::move(type))
        , data_source_(std::move(data_source))
    {
    }

    void setSize(size_t new_size);

    friend class RowBatch;
};

} // namespace velodb
