#pragma once

#include "common/copy_traits.hpp"

#include <fmt/format.h>

#include <memory>
#include <string>

namespace velodb {

enum class DataTypeId : uint32_t {
    INVALID = 0,
    BOOLEAN,
    TINYINT,
    SMALLINT,
    INTEGER,
    BIGINT,
    FLOAT,
    DOUBLE,
    VARCHAR,
    CHAR,
    DATE,
    TIMESTAMP,
    DECIMAL,
    ANY
};

class DataType : public UniqueCloneable<DataType> {
public:
    explicit DataType(DataTypeId type_id, size_t size = 0);
    virtual ~DataType() = default;

    DataTypeId getTypeId() const { return type_id_; }
    size_t size() const { return size_; }
    virtual std::string toString() const = 0;
    virtual bool isNumeric() const { return false; }
    virtual bool isBoolean() const { return false; }
    virtual bool isString() const { return false; }
    virtual bool isDateTime() const { return false; }

    static std::unique_ptr<DataType> createType(DataTypeId type_id, size_t size = 0);

protected:
    DataTypeId type_id_;
    size_t size_;

private:
    friend class UniqueCloneable<DataType>;
    std::unique_ptr<DataType> cloneUniqueImpl() const;
};

// Concrete data type implementations

class BooleanType : public DataType {
public:
    BooleanType()
        : DataType(DataTypeId::BOOLEAN, sizeof(bool))
    {
    }
    std::string toString() const override { return "BOOLEAN"; }
    bool isBoolean() const override { return true; }
};

class TinyIntType : public DataType {
public:
    TinyIntType()
        : DataType(DataTypeId::TINYINT, sizeof(int8_t))
    {
    }
    std::string toString() const override { return "TINYINT"; }
    bool isNumeric() const override { return true; }
};

class SmallIntType : public DataType {
public:
    SmallIntType()
        : DataType(DataTypeId::SMALLINT, sizeof(int16_t))
    {
    }
    std::string toString() const override { return "SMALLINT"; }
    bool isNumeric() const override { return true; }
};

class IntegerType : public DataType {
public:
    IntegerType()
        : DataType(DataTypeId::INTEGER, sizeof(int32_t))
    {
    }
    std::string toString() const override { return "INTEGER"; }
    bool isNumeric() const override { return true; }
    bool isBoolean() const override { return false; }
    bool isString() const override { return false; }
};

class BigIntType : public DataType {
public:
    BigIntType()
        : DataType(DataTypeId::BIGINT, sizeof(int64_t))
    {
    }
    std::string toString() const override { return "BIGINT"; }
    bool isNumeric() const override { return true; }
};

class FloatType : public DataType {
public:
    FloatType()
        : DataType(DataTypeId::FLOAT, sizeof(float))
    {
    }
    std::string toString() const override { return "FLOAT"; }
    bool isNumeric() const override { return true; }
};

class DoubleType : public DataType {
public:
    DoubleType()
        : DataType(DataTypeId::DOUBLE, sizeof(double))
    {
    }
    std::string toString() const override { return "DOUBLE"; }
    bool isNumeric() const override { return true; }
};

class DecimalType : public DataType {
public:
    DecimalType()
        : DataType(DataTypeId::DECIMAL, sizeof(int64_t))
    {
    }
    std::string toString() const override { return "DECIMAL"; }
    bool isNumeric() const override { return true; }
};

class CharType : public DataType {
public:
    explicit CharType(size_t length)
        : DataType(DataTypeId::CHAR, sizeof(size_t))
        , length_(length)
    {
    }
    std::string toString() const override { return fmt::format("CHAR({})", length_); }
    bool isString() const override { return true; }

private:
    size_t length_;
};

class VarcharType : public DataType {
public:
    explicit VarcharType(size_t max_length)
        : DataType(DataTypeId::VARCHAR, sizeof(size_t))
        , max_length_(max_length)
    {
    }
    std::string toString() const override { return fmt::format("VARCHAR({})", max_length_); }
    bool isString() const override { return true; }

private:
    size_t max_length_;
};

class DateType : public DataType {
public:
    DateType()
        : DataType(DataTypeId::DATE, sizeof(uint32_t))
    {
    }
    std::string toString() const override { return "DATE"; }
    bool isDateTime() const override { return true; }
};

} // namespace velodb
