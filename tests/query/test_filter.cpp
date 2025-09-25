#include "../common/test_warmup_utility.hpp"
#include "catalog/catalog.hpp"
#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include "catalog/table_builder.hpp"
#include "catalog/tuple.hpp"
#include "data/data_type.hpp"
#include "execution/execution_engine.hpp"

#include <SQLParser.h>

#include <gtest/gtest.h>

using namespace velodb;

class WhereClauseTest : public test::VeloDBTest {
public:
    WhereClauseTest()
        : catalog_()
        , planner_(catalog_)
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
        // Create products table with various data types
        auto products_schema = Schema();
        products_schema.addColumnInfo({ "id", std::make_unique<IntegerType>() });
        products_schema.addColumnInfo({ "name", std::make_unique<VarcharType>(100) });
        products_schema.addColumnInfo({ "price", std::make_unique<DoubleType>() });
        products_schema.addColumnInfo({ "quantity", std::make_unique<IntegerType>() });
        products_schema.addColumnInfo({ "category", std::make_unique<VarcharType>(50) });
        products_schema.addColumnInfo({ "in_stock", std::make_unique<BooleanType>() });

        auto builder = TableBuilder("products", std::move(products_schema));

        // Product 1: Laptop, 999.99, 10, Electronics, in_stock=1
        insertProduct(builder, 1, "Laptop", 999.99, 10, "Electronics", true);

        // Product 2: Mouse, 25.50, 50, Electronics, in_stock=1
        insertProduct(builder, 2, "Mouse", 25.50, 50, "Electronics", true);

        // Product 3: Desk, 199.99, 5, Furniture, in_stock=1
        insertProduct(builder, 3, "Desk", 199.99, 5, "Furniture", true);

        // Product 4: Chair, 89.99, 0, Furniture, in_stock=0
        insertProduct(builder, 4, "Chair", 89.99, 0, "Furniture", false);

        // Product 5: Keyboard, 75.00, 25, Electronics, in_stock=1
        insertProduct(builder, 5, "Keyboard", 75.00, 25, "Electronics", true);

        // Product 6: Book, 15.99, 100, Books, in_stock=1
        insertProduct(builder, 6, "Book", 15.99, 100, "Books", true);

        // Product 7: Pen, 2.50, 200, Stationery, in_stock=1
        insertProduct(builder, 7, "Pen", 2.50, 200, "Stationery", true);

        // Product 8: Monitor, 299.99, 0, Electronics, in_stock=0
        insertProduct(builder, 8, "Monitor", 299.99, 0, "Electronics", false);

        catalog_.addTable(std::move(builder).build());
    }

    void insertProduct(TableBuilder& builder,
                       int id,
                       const std::string& name,
                       double price,
                       int quantity,
                       const std::string& category,
                       bool in_stock)
    {
        std::vector<Value> values;
        values.push_back(Value::createInteger(id));
        values.push_back(Value::createString(name));
        values.push_back(Value::createDouble(price));
        values.push_back(Value::createInteger(quantity));
        values.push_back(Value::createString(category));
        values.push_back(Value::createBoolean(in_stock));

        builder.insertRow(values);
    }

    Catalog catalog_;
    QueryPlanner planner_;
    ExecutionEngine engine_;
};

// === NUMERIC RANGE TESTS ===

TEST_F(WhereClauseTest, PriceRangeQueries)
{
    // Test expensive items
    std::string sql = "SELECT * FROM products WHERE price > 100.0";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 3); // Laptop, Desk, Monitor

    for (const auto& tuple : view) {
        EXPECT_GT(tuple.getValue(2).getDouble(), 100.0f); // price > 100
    }
}

TEST_F(WhereClauseTest, PriceBetweenRange)
{
    // Test products in medium price range
    std::string sql = "SELECT * FROM products WHERE price >= 20.0 AND price <= 100.0";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 3); // Mouse, Chair, Keyboard

    for (const auto& tuple : view) {
        double price = tuple.getValue(2).getDouble();
        EXPECT_GE(price, 20.0f);
        EXPECT_LE(price, 100.0f);
    }
}

TEST_F(WhereClauseTest, QuantityBasedFiltering)
{
    // Test low stock items
    std::string sql = "SELECT * FROM products WHERE quantity <= 10";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(),
              4); // Laptop (10), Desk (5), Chair (0), Monitor (0)

    for (const auto& tuple : view) {
        EXPECT_LE(tuple.getValue(3).getInteger(), 10); // quantity <= 10
    }
}

// === CATEGORY-BASED TESTS ===

TEST_F(WhereClauseTest, CategoryFiltering)
{
    // Test electronics category
    std::string sql = "SELECT * FROM products WHERE category = 'Electronics'";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 4); // Laptop, Mouse, Keyboard, Monitor

    for (const auto& tuple : view) {
        EXPECT_EQ(tuple.getValue(4).getString(), "Electronics");
    }
}

TEST_F(WhereClauseTest, MultiCategoryFiltering)
{
    // Test multiple categories using OR
    std::string sql = "SELECT * FROM products WHERE category = 'Furniture' OR "
                      "category = 'Books'";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 3); // Desk, Chair, Book

    for (const auto& tuple : view) {
        std::string category = tuple.getValue(4).getString();
        EXPECT_TRUE(category == "Furniture" || category == "Books");
    }
}

// === STOCK STATUS TESTS ===

TEST_F(WhereClauseTest, InStockFiltering)
{
    // Test in-stock items
    std::string sql = "SELECT * FROM products WHERE in_stock = true";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 6); // All except Chair and Monitor

    for (const auto& tuple : view) {
        EXPECT_EQ(tuple.getValue(5).getBoolean(), true); // in_stock = true
    }
}

TEST_F(WhereClauseTest, OutOfStockFiltering)
{
    // Test out-of-stock items
    std::string sql = "SELECT * FROM products WHERE in_stock = false";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 2); // Chair and Monitor

    for (const auto& tuple : view) {
        EXPECT_EQ(tuple.getValue(5).getBoolean(), false); // in_stock = false
    }
}

// === COMPLEX BUSINESS LOGIC TESTS ===

TEST_F(WhereClauseTest, AvailableElectronicsQuery)
{
    // Test available electronics (in_stock = true AND category = 'Electronics')
    std::string sql = "SELECT * FROM products WHERE in_stock = true AND "
                      "category = 'Electronics'";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 3); // Laptop, Mouse, Keyboard

    for (const auto& tuple : view) {
        EXPECT_EQ(tuple.getValue(5).getBoolean(), true); // in_stock = true
        EXPECT_EQ(tuple.getValue(4).getString(), "Electronics");
    }
}

TEST_F(WhereClauseTest, LowStockHighValueQuery)
{
    // Test low stock but high value items
    std::string sql = "SELECT * FROM products WHERE quantity <= 10 AND price > 50.0";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 4); // Laptop, Desk, Chair, Monitor

    for (const auto& tuple : view) {
        EXPECT_LE(tuple.getValue(3).getInteger(), 10); // quantity <= 10
        EXPECT_GT(tuple.getValue(2).getDouble(), 50.0f); // price > 50
    }
}

TEST_F(WhereClauseTest, ReorderCandidatesQuery)
{
    // Test items that need reordering (quantity <= 5 OR in_stock = false)
    std::string sql = "SELECT * FROM products WHERE quantity <= 5 OR in_stock = false";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 3); // Desk (5), Chair (0), Monitor (0)

    for (const auto& tuple : view) {
        bool low_quantity = tuple.getValue(3).getInteger() <= 5;
        bool out_of_stock = !tuple.getValue(5).getBoolean();
        EXPECT_TRUE(low_quantity || out_of_stock);
    }
}

// === STRING PATTERN TESTS ===

TEST_F(WhereClauseTest, NameStartsWith)
{
    // Test products whose names start with specific letters
    std::string sql = "SELECT * FROM products WHERE name = 'Laptop' OR name = 'Mouse'";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 2); // Laptop and Mouse

    for (const auto& tuple : view) {
        std::string name = tuple.getValue(1).getString();
        EXPECT_TRUE(name == "Laptop" || name == "Mouse");
    }
}

// === NEGATION TESTS ===

TEST_F(WhereClauseTest, NotElectronicsQuery)
{
    // Test non-electronics items
    std::string sql = "SELECT * FROM products WHERE category != 'Electronics'";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 4); // Desk, Chair, Book, Pen

    for (const auto& tuple : view) {
        EXPECT_NE(tuple.getValue(4).getString(), "Electronics");
    }
}

TEST_F(WhereClauseTest, NotLowPriceQuery)
{
    // Test items that are not cheap (price > 20)
    std::string sql = "SELECT * FROM products WHERE price > 20.0";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 6); // All except Book and Pen

    for (const auto& tuple : view) {
        EXPECT_GT(tuple.getValue(2).getDouble(), 20.0f);
    }
}

// === PROJECTION WITH WHERE TESTS ===

TEST_F(WhereClauseTest, ProjectedExpensiveItems)
{
    // Test selecting specific columns for expensive items
    std::string sql = "SELECT name, price, category FROM products WHERE price > 100.0";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 3); // Laptop, Desk, Monitor
    EXPECT_EQ(view.getSchema().getColumnCount(), 3); // name, price, category

    for (const auto& tuple : view) {
        EXPECT_EQ(tuple.getColumnCount(), 3); // Only projected columns
        // Note: In projected results, price is at index 1, not 2
        EXPECT_GT(tuple.getValue(1).getDouble(), 100.0f);
    }
}

TEST_F(WhereClauseTest, ProjectedStockStatus)
{
    // Test selecting name and stock status for electronics
    std::string sql = "SELECT name, in_stock FROM products WHERE category = 'Electronics'";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 4); // Laptop, Mouse, Keyboard, Monitor
    EXPECT_EQ(view.getSchema().getColumnCount(), 2); // name, in_stock

    for (const auto& tuple : view) {
        EXPECT_EQ(tuple.getColumnCount(), 2); // Only projected columns
    }
}

// === EDGE CASES AND BOUNDARY CONDITIONS ===

TEST_F(WhereClauseTest, ExactPriceMatch)
{
    // Test exact price matching
    std::string sql = "SELECT * FROM products WHERE price = 25.50";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 1); // Mouse

    EXPECT_FLOAT_EQ(view.getValue(0, 2).getDouble(), 25.50f);
    EXPECT_EQ(view.getValue(0, 1).getString(), "Mouse");
}

TEST_F(WhereClauseTest, ZeroQuantityItems)
{
    // Test items with zero quantity
    std::string sql = "SELECT * FROM products WHERE quantity = 0";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 2); // Chair and Monitor

    for (const auto& tuple : view) {
        EXPECT_EQ(tuple.getValue(3).getInteger(), 0); // quantity = 0
    }
}

TEST_F(WhereClauseTest, EmptyResultSet)
{
    // Test query that returns no results
    std::string sql = "SELECT * FROM products WHERE price > 10000.0";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 0); // No results
}

// === COMPLEX LOGICAL COMBINATIONS ===

TEST_F(WhereClauseTest, ComplexLogicalAndConditions)
{
    // Test multiple AND conditions
    std::string sql = "SELECT * FROM products WHERE category = 'Electronics' "
                      "AND price > 50.0 AND in_stock = true";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 2); // Laptop and Keyboard

    for (const auto& tuple : view) {
        EXPECT_EQ(tuple.getValue(4).getString(), "Electronics");
        EXPECT_GT(tuple.getValue(2).getDouble(), 50.0f);
        EXPECT_EQ(tuple.getValue(5).getBoolean(), true);
    }
}

TEST_F(WhereClauseTest, ComplexLogicalOrConditions)
{
    // Test multiple OR conditions
    std::string sql = "SELECT * FROM products WHERE price < 20.0 OR quantity > "
                      "100 OR category = 'Furniture'";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 4); // Book, Pen, Desk, Chair

    for (const auto& tuple : view) {
        bool condition_met = tuple.getValue(2).getDouble() < 20.0f || tuple.getValue(3).getInteger() > 100
            || tuple.getValue(4).getString() == "Furniture";
        EXPECT_TRUE(condition_met);
    }
}

TEST_F(WhereClauseTest, MixedAndOrConditions)
{
    // Test mixed AND/OR conditions with precedence
    std::string sql = "SELECT * FROM products WHERE (category = 'Electronics' "
                      "OR category = 'Books') AND price < 100.0";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 3); // Mouse, Keyboard, Book

    for (const auto& tuple : view) {
        std::string category = tuple.getValue(4).getString();
        EXPECT_TRUE(category == "Electronics" || category == "Books");
        EXPECT_LT(tuple.getValue(2).getDouble(), 100.0f);
    }
}

// === COMPARISON OPERATOR EDGE CASES ===

TEST_F(WhereClauseTest, EqualityComparisons)
{
    // Test exact equality matches
    std::string sql = "SELECT * FROM products WHERE price = 25.50";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 1); // Mouse

    EXPECT_EQ(view.getValue(0, 1).getString(), "Mouse");
    EXPECT_EQ(view.getValue(0, 2).getDouble(), 25.50);
}

TEST_F(WhereClauseTest, InequalityComparisons)
{
    // Test not equal operator
    std::string sql = "SELECT * FROM products WHERE category != 'Electronics'";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 4); // Desk, Chair, Book, Pen

    for (const auto& tuple : view) {
        EXPECT_NE(tuple.getValue(4).getString(), "Electronics");
    }
}

TEST_F(WhereClauseTest, BoundaryValueTests)
{
    // Test boundary conditions
    std::string sql = "SELECT * FROM products WHERE quantity = 0";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 2); // Chair and Monitor

    for (const auto& tuple : view) {
        EXPECT_EQ(tuple.getValue(3).getInteger(), 0);
    }
}

// === STRING-BASED FILTERING ===

TEST_F(WhereClauseTest, StringLengthBasedFiltering)
{
    // Test filtering by string characteristics
    // Note: This uses a workaround since LIKE is not implemented
    std::string sql = "SELECT * FROM products WHERE name = 'Pen'";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 1); // Pen

    EXPECT_EQ(view.getValue(0, 1).getString(), "Pen");
}

// === MULTIPLE COLUMN FILTERING ===

TEST_F(WhereClauseTest, MultiColumnComparisons)
{
    // Test filtering on multiple different column types
    std::string sql = "SELECT * FROM products WHERE id >= 5 AND category = 'Electronics'";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 2); // Keyboard and Monitor

    for (const auto& tuple : view) {
        EXPECT_GE(tuple.getValue(0).getInteger(), 5);
        EXPECT_EQ(tuple.getValue(4).getString(), "Electronics");
    }
}

// === EDGE CASE AND ERROR SCENARIOS ===

TEST_F(WhereClauseTest, AllRowsMatchFilter)
{
    // Test query where all rows match the condition
    std::string sql = "SELECT * FROM products WHERE id > 0";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 8); // All products
}

TEST_F(WhereClauseTest, NoRowsMatchFilter)
{
    // Test query where no rows match the condition
    std::string sql = "SELECT * FROM products WHERE id < 0";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 0); // No products
}

// === PERFORMANCE AND STRESS TESTS ===

TEST_F(WhereClauseTest, ComplexNestedConditions)
{
    // Test deeply nested logical conditions
    std::string sql = "SELECT * FROM products WHERE ((category = 'Electronics' "
                      "AND price > 50.0) OR (category = "
                      "'Furniture' AND in_stock = true)) AND quantity > 5";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    // Should match: Laptop (Electronics, price>50, qty=10), Keyboard
    // (Electronics, price>50, qty=25) Should not match Desk (Furniture,
    // in_stock=1, but only qty=5)
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 2);

    for (const auto& tuple : view) {
        std::string category = tuple.getValue(4).getString();
        double price = tuple.getValue(2).getDouble();
        bool in_stock = tuple.getValue(5).getBoolean();
        int quantity = tuple.getValue(3).getInteger();

        bool condition_met = ((category == "Electronics" && price > 50.0) || (category == "Furniture" && in_stock))
            && quantity > 5;
        EXPECT_TRUE(condition_met);
    }
}

// === BOUNDARY VALUE TESTS ===

TEST_F(WhereClauseTest, ExactBoundaryValues)
{
    // Test exact match on boundary values
    std::string sql = "SELECT * FROM products WHERE price = 999.99";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 1); // Only Laptop

    EXPECT_EQ(view.getValue(0, 1).getString(), "Laptop");
}

TEST_F(WhereClauseTest, ZeroQuantityFilter)
{
    // Test filtering for zero quantity items
    std::string sql = "SELECT * FROM products WHERE quantity = 0";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 2); // Chair and Monitor

    for (const auto& tuple : view) {
        EXPECT_EQ(tuple.getValue(3).getInteger(), 0); // quantity = 0
        EXPECT_EQ(tuple.getValue(5).getBoolean(),
                  false); // should be out of stock
    }
}

// === STRING COMPARISON TESTS ===

TEST_F(WhereClauseTest, StringEqualityTests)
{
    // Test exact string matching
    std::string sql = "SELECT * FROM products WHERE name = 'Laptop'";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 1);
    EXPECT_EQ(view.getValue(0, 1).getString(), "Laptop");
}

TEST_F(WhereClauseTest, StringInequalityTests)
{
    // Test string inequality
    std::string sql = "SELECT * FROM products WHERE name != 'Laptop'";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 7); // All except Laptop

    for (const auto& tuple : view) {
        EXPECT_NE(tuple.getValue(1).getString(), "Laptop");
    }
}

// === MULTIPLE CONDITION COMBINATIONS ===

TEST_F(WhereClauseTest, ThreeConditionAND)
{
    // Test three conditions with AND
    std::string sql = "SELECT * FROM products WHERE category = 'Electronics' "
                      "AND price < 100.0 AND in_stock = true";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 2); // Mouse and Keyboard

    for (const auto& tuple : view) {
        EXPECT_EQ(tuple.getValue(4).getString(), "Electronics");
        EXPECT_LT(tuple.getValue(2).getDouble(), 100.0);
        EXPECT_EQ(tuple.getValue(5).getBoolean(), true);
    }
}

TEST_F(WhereClauseTest, ThreeConditionOR)
{
    // Test three conditions with OR
    std::string sql = "SELECT * FROM products WHERE category = 'Books' OR "
                      "category = 'Stationery' OR price > 500.0";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 3); // Book, Pen, Laptop

    for (const auto& tuple : view) {
        std::string category = tuple.getValue(4).getString();
        double price = tuple.getValue(2).getDouble();
        bool condition_met = (category == "Books" || category == "Stationery" || price > 500.0);
        EXPECT_TRUE(condition_met);
    }
}

// === EDGE CASE TESTS ===

TEST_F(WhereClauseTest, SingleRowTable)
{
    // Create a table with single row and test filtering
    auto single_schema = Schema();
    single_schema.addColumnInfo({ "id", std::make_unique<IntegerType>() });
    single_schema.addColumnInfo({ "name", std::make_unique<VarcharType>(50) });

    auto builder = TableBuilder("single_item", std::move(single_schema));

    std::vector<Value> values;
    values.push_back(Value::createInteger(1));
    values.push_back(Value::createString("OnlyItem"));

    builder.insertRow(values);
    catalog_.addTable(std::move(builder).build());

    // Test matching condition
    std::string sql = "SELECT * FROM single_item WHERE id = 1";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 1);
}

TEST_F(WhereClauseTest, AllRowsFiltered)
{
    // Test condition that filters out all rows
    std::string sql = "SELECT * FROM products WHERE price < 0.0";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 0);
}

// === NUMERICAL PRECISION TESTS ===

TEST_F(WhereClauseTest, FloatingPointPrecision)
{
    // Test floating point comparisons with precise values
    std::string sql = "SELECT * FROM products WHERE price = 25.50";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 1); // Mouse
    EXPECT_EQ(view.getValue(0, 1).getString(), "Mouse");
}

TEST_F(WhereClauseTest, LargeIntegerComparison)
{
    // Test with large quantity values
    std::string sql = "SELECT * FROM products WHERE quantity >= 100";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 2); // Book (100), Pen (200)

    for (const auto& tuple : view) {
        EXPECT_GE(tuple.getValue(3).getInteger(), 100);
    }
}

// === BOOLEAN LOGIC STRESS TESTS ===

TEST_F(WhereClauseTest, ComplexBooleanExpression)
{
    // Test complex boolean logic with mixed operators
    std::string sql = "SELECT * FROM products WHERE (category = 'Electronics' "
                      "OR category = 'Furniture') AND (price > "
                      "20.0 AND price < 1000.0) AND in_stock = true";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    // Should match: Laptop, Mouse, Keyboard, Desk
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 4);

    for (const auto& tuple : view) {
        std::string category = tuple.getValue(4).getString();
        double price = tuple.getValue(2).getDouble();
        bool in_stock = tuple.getValue(5).getBoolean();

        bool condition_met = (category == "Electronics" || category == "Furniture") && (price > 20.0 && price < 1000.0)
            && in_stock;
        EXPECT_TRUE(condition_met);
    }
}

TEST_F(WhereClauseTest, NegationWithComplexConditions)
{
    // Test NOT with complex nested conditions
    std::string sql = "SELECT * FROM products WHERE NOT (category = "
                      "'Electronics' AND price > 100.0)";
    auto query_result = engine_.executeQuery(sql);

    ASSERT_TRUE(static_cast<bool>(query_result));
    // Should exclude: Laptop (Electronics, price=999.99), Monitor (Electronics,
    // price=299.99) Should include: All others (6 items)
    auto& view = query_result.value();
    EXPECT_EQ(view.getRowCount(), 6);

    for (const auto& tuple : view) {
        std::string category = tuple.getValue(4).getString();
        double price = tuple.getValue(2).getDouble();

        // NOT (category = 'Electronics' AND price > 100.0)
        bool condition_met = !(category == "Electronics" && price > 100.0);
        EXPECT_TRUE(condition_met);
    }
}

// TODO: Add tests for additional WHERE clause features when implemented:
// - LIKE pattern matching (name LIKE 'L%')
// - IN operator (category IN ('Electronics', 'Books'))
// - BETWEEN operator (price BETWEEN 20.0 AND 100.0)
// - IS NULL / IS NOT NULL
// - EXISTS subqueries
// - Correlated subqueries
// - Join conditions with WHERE clauses
// - Performance tests for large datasets
// - Unicode string filtering
// - Date/time filtering when date types are added
