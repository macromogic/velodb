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

    [[nodiscard]] DataTypeId getTypeId() const { return type_id_; }
    [[nodiscard]] size_t getSize() const { return size_; }
    [[nodiscard]] virtual std::string toString() const = 0;
    [[nodiscard]] virtual bool isFixedSize() const = 0;
    [[nodiscard]] virtual bool isNumeric() const = 0;

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
    [[nodiscard]] std::string toString() const override { return "BOOLEAN"; }
    [[nodiscard]] bool isFixedSize() const override { return true; }
    [[nodiscard]] bool isNumeric() const override { return false; }
};

class IntegerType : public DataType {
public:
    IntegerType()
        : DataType(DataTypeId::INTEGER, sizeof(int32_t))
    {
    }
    [[nodiscard]] std::string toString() const override { return "INTEGER"; }
    [[nodiscard]] bool isFixedSize() const override { return true; }
    [[nodiscard]] bool isNumeric() const override { return true; }
};

class BigIntType : public DataType {
public:
    BigIntType()
        : DataType(DataTypeId::BIGINT, sizeof(int64_t))
    {
    }
    [[nodiscard]] std::string toString() const override { return "BIGINT"; }
    [[nodiscard]] bool isFixedSize() const override { return true; }
    [[nodiscard]] bool isNumeric() const override { return true; }
};

class DoubleType : public DataType {
public:
    DoubleType()
        : DataType(DataTypeId::DOUBLE, sizeof(double))
    {
    }
    [[nodiscard]] std::string toString() const override { return "DOUBLE"; }
    [[nodiscard]] bool isFixedSize() const override { return true; }
    [[nodiscard]] bool isNumeric() const override { return true; }
};

class VarcharType : public DataType {
public:
    explicit VarcharType(size_t max_length)
        : DataType(DataTypeId::VARCHAR, max_length)
    {
    }
    [[nodiscard]] std::string toString() const override { return "VARCHAR(" + std::to_string(size_) + ")"; }
    [[nodiscard]] bool isFixedSize() const override { return false; }
    [[nodiscard]] bool isNumeric() const override { return false; }
};

} // namespace velodb
