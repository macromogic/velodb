#include <gtest/gtest.h>
#include "types/value.hpp"
#include "types/data_type.hpp"

using namespace velodb;

class ValueTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

TEST_F(ValueTest, CreateIntegerValue) {
    Value int_val = Value::createInteger(42);
    
    EXPECT_EQ(int_val.getTypeId(), DataTypeId::INTEGER);
    EXPECT_FALSE(int_val.isNull());
    EXPECT_EQ(int_val.getInteger(), 42);
    EXPECT_EQ(int_val.toString(), "42");
}

TEST_F(ValueTest, CreateDoubleValue) {
    Value double_val = Value::createDouble(3.14);

    EXPECT_EQ(double_val.getTypeId(), DataTypeId::DOUBLE);
    EXPECT_FALSE(double_val.isNull());
    EXPECT_DOUBLE_EQ(double_val.getDouble(), 3.14);
    EXPECT_EQ(double_val.toString(), "3.14");
}

TEST_F(ValueTest, CreateStringValue) {
    Value string_val = Value::createString("hello");

    EXPECT_EQ(string_val.getTypeId(), DataTypeId::VARCHAR);
    EXPECT_FALSE(string_val.isNull());
    EXPECT_EQ(string_val.getString(), "hello");
    EXPECT_EQ(string_val.toString(), "hello");
}

TEST_F(ValueTest, CreateBooleanValue) {
    Value bool_val = Value::createBoolean(true);

    EXPECT_EQ(bool_val.getTypeId(), DataTypeId::BOOLEAN);
    EXPECT_FALSE(bool_val.isNull());
    EXPECT_TRUE(bool_val.getBoolean());
    EXPECT_EQ(bool_val.toString(), "true");

    Value false_val = Value::createBoolean(false);
    EXPECT_FALSE(false_val.getBoolean());
    EXPECT_EQ(false_val.toString(), "false");
}

TEST_F(ValueTest, CreateNullValue) {
    Value null_val = Value::createNull(DataTypeId::INTEGER);

    EXPECT_EQ(null_val.getTypeId(), DataTypeId::INTEGER);
    EXPECT_TRUE(null_val.isNull());
    EXPECT_EQ(null_val.toString(), "NULL");
}

TEST_F(ValueTest, ValueCopyAndMove) {
    Value original = Value::createInteger(100);

    // Test copy constructor
    Value copied = original;
    EXPECT_EQ(copied.getInteger(), 100);
    EXPECT_FALSE(copied.isNull());

    // Test move constructor
    Value moved = std::move(original);
    EXPECT_EQ(moved.getInteger(), 100);
    EXPECT_FALSE(moved.isNull());
}

TEST_F(ValueTest, ValueAssignment) {
    Value val1 = Value::createInteger(10);
    Value val2 = Value::createInteger(20);
    
    val1 = val2;
    EXPECT_EQ(val1.getInteger(), 20);

    val1 = Value::createString("test");
    EXPECT_EQ(val1.getString(), "test");
    EXPECT_EQ(val1.getTypeId(), DataTypeId::VARCHAR);
}

TEST_F(ValueTest, ValueComparison) {
    Value val1 = Value::createInteger(42);
    Value val2 = Value::createInteger(42);
    Value val3 = Value::createInteger(24);

    EXPECT_TRUE(val1 == val2);
    EXPECT_FALSE(val1 == val3);
    EXPECT_TRUE(val1 != val3);

    Value null_val = Value::createNull(DataTypeId::INTEGER);
    EXPECT_FALSE(val1 == null_val);
    EXPECT_FALSE(null_val == val1);
    EXPECT_TRUE(val1 != null_val);
}

TEST_F(ValueTest, ValueTypeConversion) {
    // Test that we can create values of different types
    Value int_val = Value::createInteger(123);
    Value double_val = Value::createDouble(123.0);
    Value string_val = Value::createString("123");

    EXPECT_EQ(int_val.getTypeId(), DataTypeId::INTEGER);
    EXPECT_EQ(double_val.getTypeId(), DataTypeId::DOUBLE);
    EXPECT_EQ(string_val.getTypeId(), DataTypeId::VARCHAR);

    // Values should not be equal even if they represent the same number
    EXPECT_FALSE(int_val == double_val);
    EXPECT_FALSE(int_val == string_val);
    EXPECT_TRUE(int_val != double_val);
}

TEST_F(ValueTest, ValueOrdering) {
    Value val1 = Value::createInteger(10);
    Value val2 = Value::createInteger(20);
    Value val3 = Value::createInteger(10);

    EXPECT_TRUE(val1 < val2);
    EXPECT_FALSE(val2 < val1);
    EXPECT_TRUE(val1 <= val2);
    EXPECT_TRUE(val1 <= val3);
    EXPECT_TRUE(val2 > val1);
    EXPECT_FALSE(val1 > val2);
    EXPECT_TRUE(val2 >= val1);
    EXPECT_TRUE(val1 >= val3);
}
