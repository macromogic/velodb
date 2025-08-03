#pragma once

#include "data_type.hpp"
#include <memory>
#include <string>
#include <variant>

namespace velodb {

// Forward declarations
class Value;

// Value variant type for storing actual data
using ValueData = std::variant<
    bool,
    int8_t,
    int16_t,
    int32_t,
    int64_t,
    float,
    double,
    std::string>;

class Value {
public:
    Value()
        : type_id_(DataTypeId::INVALID)
        , is_null_(true)
    {
    }
    Value(DataTypeId type_id, ValueData data);
    explicit Value(DataTypeId type_id); // Creates NULL value

    ~Value() = default;

    // Copy and move constructors
    Value(const Value& other) = default;
    Value(Value&& other) = default;
    Value& operator=(const Value& other) = default;
    Value& operator=(Value&& other) = default;

    // Type information
    DataTypeId getTypeId() const { return type_id_; }
    bool isNull() const { return is_null_; }

    bool getBoolean() const;
    int8_t getTinyInt() const;
    int16_t getSmallInt() const;
    int32_t getInteger() const;
    int64_t getBigInt() const;
    float getFloat() const;
    double getDouble() const;
    std::string getString() const;

    // Comparison operators
    bool operator==(const Value& other) const;
    bool operator!=(const Value& other) const;
    bool operator<(const Value& other) const;
    bool operator<=(const Value& other) const;
    bool operator>(const Value& other) const;
    bool operator>=(const Value& other) const;

    // String representation
    std::string toString() const;

    // Static factory methods
    static Value createBoolean(bool value);
    static Value createTinyInt(int8_t value);
    static Value createSmallInt(int16_t value);
    static Value createInteger(int32_t value);
    static Value createBigInt(int64_t value);
    static Value createFloat(float value);
    static Value createDouble(double value);
    static Value createString(const std::string& value);
    static Value createNull(DataTypeId type_id);

private:
    DataTypeId type_id_;
    ValueData data_;
    bool is_null_;
};

// Vector of values for columnar storage
using ValueVector = std::vector<Value>;

} // namespace velodb
