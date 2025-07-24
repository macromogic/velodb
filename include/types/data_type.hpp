#pragma once

#include <memory>
#include <string>
#include <vector>

namespace velodb {

enum class DataTypeId {
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
    DECIMAL
};

class DataType {
public:
    explicit DataType(DataTypeId type_id, size_t size = 0);
    virtual ~DataType() = default;

    DataTypeId getTypeId() const { return type_id_; }
    size_t getSize() const { return size_; }
    virtual std::string toString() const = 0;
    virtual bool isFixedSize() const = 0;
    virtual bool isNumeric() const = 0;

    std::unique_ptr<DataType> clone() const {
        return std::unique_ptr<DataType>(createType(type_id_, size_));
    }

    static std::unique_ptr<DataType> createType(DataTypeId type_id, size_t size = 0);

protected:
    DataTypeId type_id_;
    size_t size_;
};

// Concrete data type implementations

class BooleanType : public DataType {
public:
    BooleanType()
        : DataType(DataTypeId::BOOLEAN, sizeof(bool))
    {
    }
    std::string toString() const override { return "BOOLEAN"; }
    bool isFixedSize() const override { return true; }
    bool isNumeric() const override { return false; }
};

class IntegerType : public DataType {
public:
    IntegerType()
        : DataType(DataTypeId::INTEGER, sizeof(int32_t))
    {
    }
    std::string toString() const override { return "INTEGER"; }
    bool isFixedSize() const override { return true; }
    bool isNumeric() const override { return true; }
};

class BigIntType : public DataType {
public:
    BigIntType()
        : DataType(DataTypeId::BIGINT, sizeof(int64_t))
    {
    }
    std::string toString() const override { return "BIGINT"; }
    bool isFixedSize() const override { return true; }
    bool isNumeric() const override { return true; }
};

class DoubleType : public DataType {
public:
    DoubleType()
        : DataType(DataTypeId::DOUBLE, sizeof(double))
    {
    }
    std::string toString() const override { return "DOUBLE"; }
    bool isFixedSize() const override { return true; }
    bool isNumeric() const override { return true; }
};

class VarcharType : public DataType {
public:
    explicit VarcharType(size_t max_length)
        : DataType(DataTypeId::VARCHAR, max_length)
    {
    }
    std::string toString() const override { return "VARCHAR(" + std::to_string(size_) + ")"; }
    bool isFixedSize() const override { return false; }
    bool isNumeric() const override { return false; }
};

} // namespace velodb
