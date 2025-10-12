#include "../common/test_warmup_utility.hpp"
#include "catalog/catalog.hpp"
#include "catalog/execution_context.hpp"
#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include "catalog/table_builder.hpp"
#include "data/data_type.hpp"
#include "execution/execution_engine.hpp"

#include <SQLParser.h>

#include <gtest/gtest.h>

using namespace velodb;

class OrderByTest : public test::VeloDBTest {
public:
    OrderByTest()
        : catalog_()
        , engine_(catalog_)
    {
    }

protected:
    void SetUp() override
    {
        test::VeloDBTest::SetUp();
        setupTestTables();
    }

    void setupTestTables()
    {
        // Create employees table with various data types for comprehensive ORDER BY testing
        auto employees_schema = Schema();
        employees_schema.addColumnInfo({ "id", std::make_unique<IntegerType>() });
        employees_schema.addColumnInfo({ "name", std::make_unique<VarcharType>(100) });
        employees_schema.addColumnInfo({ "salary", std::make_unique<DoubleType>() });
        employees_schema.addColumnInfo({ "department", std::make_unique<VarcharType>(50) });
        employees_schema.addColumnInfo({ "age", std::make_unique<IntegerType>() });
        employees_schema.addColumnInfo({ "active", std::make_unique<BooleanType>() });

        auto builder = TableBuilder("employees", std::move(employees_schema));

        // Insert test data with intentionally mixed order for sorting validation
        insertEmployee(builder, 3, "Charlie", 75000.0, "Engineering", 28, true);
        insertEmployee(builder, 1, "Alice", 50000.0, "Marketing", 30, true);
        insertEmployee(builder, 5, "Eve", 90000.0, "Engineering", 35, false);
        insertEmployee(builder, 2, "Bob", 60000.0, "Sales", 25, true);
        insertEmployee(builder, 4, "David", 75000.0, "Marketing", 32, true);
        insertEmployee(builder, 6, "Frank", 45000.0, "Sales", 28, false);
        insertEmployee(builder, 7, "Grace", 80000.0, "Engineering", 29, true);
        insertEmployee(builder, 8, "Helen", 55000.0, "Marketing", 26, true);

        catalog_.addTable(std::move(builder).build());

        // Create a products table for testing different data types
        auto products_schema = Schema();
        products_schema.addColumnInfo({ "product_id", std::make_unique<IntegerType>() });
        products_schema.addColumnInfo({ "product_name", std::make_unique<VarcharType>(100) });
        products_schema.addColumnInfo({ "price", std::make_unique<DoubleType>() });
        products_schema.addColumnInfo({ "rating", std::make_unique<DoubleType>() });

        auto products_builder = TableBuilder("products", std::move(products_schema));

        insertProduct(products_builder, 10, "Laptop", 999.99, 4.5);
        insertProduct(products_builder, 20, "Mouse", 25.99, 4.2);
        insertProduct(products_builder, 15, "Keyboard", 89.99, 4.7);
        insertProduct(products_builder, 5, "Monitor", 299.99, 4.3);
        insertProduct(products_builder, 25, "Headphones", 79.99, 4.1);

        catalog_.addTable(std::move(products_builder).build());
    }

    void insertEmployee(TableBuilder& builder,
                        int id,
                        const std::string& name,
                        double salary,
                        const std::string& department,
                        int age,
                        bool active)
    {
        std::vector<Value> values;
        values.push_back(Value::createInteger(id));
        values.push_back(Value::createString(name));
        values.push_back(Value::createDouble(salary));
        values.push_back(Value::createString(department));
        values.push_back(Value::createInteger(age));
        values.push_back(Value::createBoolean(active));
        builder.insertRow(values);
    }

    void insertProduct(TableBuilder& builder, int id, const std::string& name, double price, double rating)
    {
        std::vector<Value> values;
        values.push_back(Value::createInteger(id));
        values.push_back(Value::createString(name));
        values.push_back(Value::createDouble(price));
        values.push_back(Value::createDouble(rating));
        builder.insertRow(values);
    }

    void TearDown() override { test::VeloDBTest::TearDown(); }

    Catalog catalog_;
    ExecutionEngine engine_;
};

// Test basic ORDER BY with integer column, ascending order (default)
TEST_F(OrderByTest, BasicOrderByIntegerAscending)
{
    std::string sql = "SELECT id, name FROM employees ORDER BY id";

    auto result = engine_.executeQuery(sql);
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();
    auto& view = result.value();

    EXPECT_EQ(view.getRowCount(), 8);

    // Verify ascending order by id
    if (view.getRowCount() >= 4) {
        EXPECT_EQ(view.getValue(0, 0).getInteger(), 1); // Alice
        EXPECT_EQ(view.getValue(1, 0).getInteger(), 2); // Bob
        EXPECT_EQ(view.getValue(2, 0).getInteger(), 3); // Charlie
        EXPECT_EQ(view.getValue(3, 0).getInteger(), 4); // David
    }
}

// Test ORDER BY with explicit ASC keyword
TEST_F(OrderByTest, OrderByExplicitAscending)
{
    std::string sql = "SELECT id, name FROM employees ORDER BY id ASC";

    auto result = engine_.executeQuery(sql);
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();
    auto& view = result.value();

    EXPECT_EQ(view.getRowCount(), 8);

    // Verify ascending order
    if (view.getRowCount() >= 3) {
        EXPECT_EQ(view.getValue(0, 0).getInteger(), 1);
        EXPECT_EQ(view.getValue(1, 0).getInteger(), 2);
        EXPECT_EQ(view.getValue(2, 0).getInteger(), 3);
    }
}

// Test ORDER BY with DESC keyword
TEST_F(OrderByTest, OrderByDescending)
{
    std::string sql = "SELECT id, name FROM employees ORDER BY id DESC";

    auto result = engine_.executeQuery(sql);
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();
    auto& view = result.value();

    EXPECT_EQ(view.getRowCount(), 8);

    // Verify descending order by id
    if (view.getRowCount() >= 4) {
        EXPECT_EQ(view.getValue(0, 0).getInteger(), 8); // Helen
        EXPECT_EQ(view.getValue(1, 0).getInteger(), 7); // Grace
        EXPECT_EQ(view.getValue(2, 0).getInteger(), 6); // Frank
        EXPECT_EQ(view.getValue(3, 0).getInteger(), 5); // Eve
    }
}

// Test ORDER BY with string column
TEST_F(OrderByTest, OrderByStringColumn)
{
    std::string sql = "SELECT name, id FROM employees ORDER BY name";

    auto result = engine_.executeQuery(sql);
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();
    auto& view = result.value();

    EXPECT_EQ(view.getRowCount(), 8);

    // Verify alphabetical order by name
    if (view.getRowCount() >= 4) {
        EXPECT_EQ(view.getValue(0, 0).getString(), "Alice");
        EXPECT_EQ(view.getValue(1, 0).getString(), "Bob");
        EXPECT_EQ(view.getValue(2, 0).getString(), "Charlie");
        EXPECT_EQ(view.getValue(3, 0).getString(), "David");
    }
}

// Test ORDER BY with double/float column
TEST_F(OrderByTest, OrderByDoubleColumn)
{
    std::string sql = "SELECT name, salary FROM employees ORDER BY salary";

    auto result = engine_.executeQuery(sql);
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();
    auto& view = result.value();

    EXPECT_EQ(view.getRowCount(), 8);

    // Verify ascending order by salary
    if (view.getRowCount() >= 4) {
        EXPECT_EQ(view.getValue(0, 1).getDouble(), 45000.0); // Frank
        EXPECT_EQ(view.getValue(1, 1).getDouble(), 50000.0); // Alice
        EXPECT_EQ(view.getValue(2, 1).getDouble(), 55000.0); // Helen
        EXPECT_EQ(view.getValue(3, 1).getDouble(), 60000.0); // Bob
    }
}

// Test ORDER BY with multiple columns
TEST_F(OrderByTest, OrderByMultipleColumns)
{
    std::string sql = "SELECT name, department, salary FROM employees ORDER BY department, salary";

    auto result = engine_.executeQuery(sql);
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();
    auto& view = result.value();

    EXPECT_EQ(view.getRowCount(), 8);

    // Verify sorting: first by department (alphabetical), then by salary (ascending)
    // Engineering: Charlie (75000), Eve (90000), Grace (80000)
    // Marketing: Alice (50000), David (75000), Helen (55000)
    // Sales: Frank (45000), Bob (60000)
    if (view.getRowCount() >= 6) {
        // First should be Engineering department with lowest salary (Charlie - 75000)
        EXPECT_EQ(view.getValue(0, 1).getString(), "Engineering");
        EXPECT_EQ(view.getValue(0, 2).getDouble(), 75000.0);

        // Then Engineering with higher salaries
        EXPECT_EQ(view.getValue(1, 1).getString(), "Engineering");
        EXPECT_EQ(view.getValue(1, 2).getDouble(), 80000.0); // Grace

        EXPECT_EQ(view.getValue(2, 1).getString(), "Engineering");
        EXPECT_EQ(view.getValue(2, 2).getDouble(), 90000.0); // Eve
    }
}

// Test ORDER BY with mixed ASC/DESC
TEST_F(OrderByTest, OrderByMixedAscDesc)
{
    std::string sql = "SELECT name, department, salary FROM employees ORDER BY department ASC, salary DESC";

    auto result = engine_.executeQuery(sql);
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();
    auto& view = result.value();

    EXPECT_EQ(view.getRowCount(), 8);

    // Verify sorting: department ascending, salary descending within each department
    if (view.getRowCount() >= 3) {
        // Engineering department: highest salary first (Eve - 90000)
        EXPECT_EQ(view.getValue(0, 1).getString(), "Engineering");
        EXPECT_EQ(view.getValue(0, 2).getDouble(), 90000.0); // Eve

        // Then Grace (80000)
        EXPECT_EQ(view.getValue(1, 1).getString(), "Engineering");
        EXPECT_EQ(view.getValue(1, 2).getDouble(), 80000.0); // Grace

        // Then Charlie (75000)
        EXPECT_EQ(view.getValue(2, 1).getString(), "Engineering");
        EXPECT_EQ(view.getValue(2, 2).getDouble(), 75000.0); // Charlie
    }
}

// Test ORDER BY with boolean column
TEST_F(OrderByTest, OrderByBooleanColumn)
{
    std::string sql = "SELECT name, active FROM employees ORDER BY active, name";

    auto result = engine_.executeQuery(sql);
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();
    auto& view = result.value();

    EXPECT_EQ(view.getRowCount(), 8);

    // Boolean values: false comes before true in ascending order
    // inactive employees first (active = false), then active employees (active = true)
    if (view.getRowCount() >= 3) {
        // First should be inactive employees (false)
        EXPECT_EQ(view.getValue(0, 1).getBoolean(), false);
        EXPECT_EQ(view.getValue(1, 1).getBoolean(), false);

        // Then active employees (true)
        EXPECT_EQ(view.getValue(2, 1).getBoolean(), true);
    }
}

// Test ORDER BY with LIMIT
TEST_F(OrderByTest, OrderByWithLimit)
{
    std::string sql = "SELECT name, salary FROM employees ORDER BY salary DESC LIMIT 3";

    auto result = engine_.executeQuery(sql);
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();
    auto& view = result.value();

    EXPECT_EQ(view.getRowCount(), 3);

    // Should get top 3 highest salaries: Eve (90000), Grace (80000), Charlie/David (75000)
    if (view.getRowCount() >= 3) {
        EXPECT_EQ(view.getValue(0, 1).getDouble(), 90000.0); // Eve
        EXPECT_EQ(view.getValue(1, 1).getDouble(), 80000.0); // Grace
        EXPECT_EQ(view.getValue(2, 1).getDouble(), 75000.0); // Charlie or David
    }
}

// Test ORDER BY with OFFSET
TEST_F(OrderByTest, OrderByWithOffset)
{
    std::string sql = "SELECT name, salary FROM employees ORDER BY salary OFFSET 2";

    auto result = engine_.executeQuery(sql);
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();
    auto& view = result.value();

    EXPECT_EQ(view.getRowCount(), 6); // 8 total - 2 offset = 6 remaining

    // Should skip first 2 lowest salaries and start from 3rd lowest
    if (view.getRowCount() >= 2) {
        EXPECT_EQ(view.getValue(0, 1).getDouble(), 55000.0); // Helen (3rd lowest)
        EXPECT_EQ(view.getValue(1, 1).getDouble(), 60000.0); // Bob (4th lowest)
    }
}

// Test ORDER BY with LIMIT and OFFSET
TEST_F(OrderByTest, OrderByWithLimitAndOffset)
{
    std::string sql = "SELECT name, salary FROM employees ORDER BY salary DESC LIMIT 2 OFFSET 1";

    auto result = engine_.executeQuery(sql);
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();
    auto& view = result.value();

    EXPECT_EQ(view.getRowCount(), 2);

    // Skip highest salary (Eve - 90000), get next 2: Grace (80000), Charlie/David (75000)
    if (view.getRowCount() >= 2) {
        EXPECT_EQ(view.getValue(0, 1).getDouble(), 80000.0); // Grace
        EXPECT_EQ(view.getValue(1, 1).getDouble(), 75000.0); // Charlie or David
    }
}

// Test ORDER BY with WHERE clause
TEST_F(OrderByTest, OrderByWithWhereClause)
{
    std::string sql = "SELECT name, salary FROM employees WHERE department = 'Engineering' ORDER BY salary DESC";

    auto result = engine_.executeQuery(sql);
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();
    auto& view = result.value();

    EXPECT_EQ(view.getRowCount(), 3); // 3 engineering employees

    // Engineering employees ordered by salary descending: Eve (90000), Grace (80000), Charlie (75000)
    if (view.getRowCount() >= 3) {
        EXPECT_EQ(view.getValue(0, 1).getDouble(), 90000.0); // Eve
        EXPECT_EQ(view.getValue(1, 1).getDouble(), 80000.0); // Grace
        EXPECT_EQ(view.getValue(2, 1).getDouble(), 75000.0); // Charlie
    }
}

// Test ORDER BY with different data types in products table
TEST_F(OrderByTest, OrderByDifferentTable)
{
    std::string sql = "SELECT product_name, price FROM products ORDER BY price";

    auto result = engine_.executeQuery(sql);
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();
    auto& view = result.value();

    EXPECT_EQ(view.getRowCount(), 5);

    // Verify ascending order by price
    if (view.getRowCount() >= 3) {
        EXPECT_EQ(view.getValue(0, 1).getDouble(), 25.99); // Mouse
        EXPECT_EQ(view.getValue(1, 1).getDouble(), 79.99); // Headphones
        EXPECT_EQ(view.getValue(2, 1).getDouble(), 89.99); // Keyboard
    }
}

// Test ORDER BY with floating point precision
TEST_F(OrderByTest, OrderByFloatingPointPrecision)
{
    std::string sql = "SELECT product_name, rating FROM products ORDER BY rating DESC";

    auto result = engine_.executeQuery(sql);
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();
    auto& view = result.value();

    EXPECT_EQ(view.getRowCount(), 5);

    // Verify descending order by rating (floating point values)
    if (view.getRowCount() >= 3) {
        EXPECT_DOUBLE_EQ(view.getValue(0, 1).getDouble(), 4.7); // Keyboard
        EXPECT_DOUBLE_EQ(view.getValue(1, 1).getDouble(), 4.5); // Laptop
        EXPECT_DOUBLE_EQ(view.getValue(2, 1).getDouble(), 4.3); // Monitor
    }
}

// Test ORDER BY with duplicate values
TEST_F(OrderByTest, OrderByWithDuplicateValues)
{
    std::string sql = "SELECT name, salary FROM employees WHERE salary = 75000.0 ORDER BY name";

    auto result = engine_.executeQuery(sql);
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();
    auto& view = result.value();

    EXPECT_EQ(view.getRowCount(), 2); // Charlie and David both have 75000 salary

    // Should be ordered alphabetically by name
    if (view.getRowCount() >= 2) {
        EXPECT_EQ(view.getValue(0, 0).getString(), "Charlie");
        EXPECT_EQ(view.getValue(1, 0).getString(), "David");
    }
}

// Test ORDER BY with all columns selected
TEST_F(OrderByTest, OrderByWithSelectAll)
{
    std::string sql = "SELECT * FROM products ORDER BY product_id DESC";

    auto result = engine_.executeQuery(sql);
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();
    auto& view = result.value();

    EXPECT_EQ(view.getRowCount(), 5);
    // TODO: temporarily include $_rowid and $_mask in count
    EXPECT_EQ(view.getSchema().getColumnCount(), 6); // All columns from products table

    // Verify descending order by product_id
    if (view.getRowCount() >= 3) {
        EXPECT_EQ(view.getValue(0, 0).getInteger(), 25); // Headphones
        EXPECT_EQ(view.getValue(1, 0).getInteger(), 20); // Mouse
        EXPECT_EQ(view.getValue(2, 0).getInteger(), 15); // Keyboard
    }
}

// Test empty result set with ORDER BY
TEST_F(OrderByTest, OrderByEmptyResultSet)
{
    std::string sql = "SELECT name, salary FROM employees WHERE salary > 100000.0 ORDER BY salary";

    auto result = engine_.executeQuery(sql);
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();
    auto& view = result.value();

    EXPECT_EQ(view.getRowCount(), 0); // No employees with salary > 100000
}

// Test ORDER BY with single row result
TEST_F(OrderByTest, OrderBySingleRow)
{
    std::string sql = "SELECT name, salary FROM employees WHERE name = 'Alice' ORDER BY salary";

    auto result = engine_.executeQuery(sql);
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();
    auto& view = result.value();

    EXPECT_EQ(view.getRowCount(), 1);

    if (view.getRowCount() >= 1) {
        EXPECT_EQ(view.getValue(0, 0).getString(), "Alice");
        EXPECT_EQ(view.getValue(0, 1).getDouble(), 50000.0);
    }
}
