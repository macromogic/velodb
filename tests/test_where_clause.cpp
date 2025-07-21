#include <gtest/gtest.h>
#include "execution/execution_engine.hpp"
#include "planner.hpp"
#include "catalog/catalog.hpp"
#include "catalog/table.hpp"
#include "catalog/schema.hpp"
#include "types/data_type.hpp"
#include "SQLParser.h"

using namespace velodb;

class WhereClauseTest : public ::testing::Test {
protected:
    void SetUp() override {
        catalog_ = std::make_unique<Catalog>();
        planner_ = std::make_unique<QueryPlanner>(catalog_.get());
        engine_ = std::make_unique<ExecutionEngine>(catalog_.get());
        
        setupTestTables();
    }

    void setupTestTables() {
        // Create products table with various data types
        auto int_type = std::make_unique<IntegerType>();
        auto varchar_type = std::make_unique<VarcharType>(100);
        auto double_type = std::make_unique<DoubleType>();
        
        Column id_col("id", std::move(int_type));
        Column name_col("name", std::move(varchar_type));
        Column price_col("price", std::move(double_type));
        Column quantity_col("quantity", std::make_unique<IntegerType>());
        Column category_col("category", std::make_unique<VarcharType>(50));
        Column in_stock_col("in_stock", std::make_unique<IntegerType>());  // 0 or 1 for boolean
        
        auto products_schema = std::make_unique<Schema>();
        products_schema->addColumn(std::move(id_col));
        products_schema->addColumn(std::move(name_col));
        products_schema->addColumn(std::move(price_col));
        products_schema->addColumn(std::move(quantity_col));
        products_schema->addColumn(std::move(category_col));
        products_schema->addColumn(std::move(in_stock_col));
        
        catalog_->createTable("products", std::move(products_schema));
        
        // Insert test data
        TableBase* table = catalog_->getTable("products");
        Table* concrete_table = static_cast<Table*>(table);
        
        // Product 1: Laptop, 999.99, 10, Electronics, in_stock=1
        insertProduct(concrete_table, 1, "Laptop", 999.99, 10, "Electronics", 1);
        
        // Product 2: Mouse, 25.50, 50, Electronics, in_stock=1
        insertProduct(concrete_table, 2, "Mouse", 25.50, 50, "Electronics", 1);
        
        // Product 3: Desk, 199.99, 5, Furniture, in_stock=1
        insertProduct(concrete_table, 3, "Desk", 199.99, 5, "Furniture", 1);
        
        // Product 4: Chair, 89.99, 0, Furniture, in_stock=0
        insertProduct(concrete_table, 4, "Chair", 89.99, 0, "Furniture", 0);
        
        // Product 5: Keyboard, 75.00, 25, Electronics, in_stock=1
        insertProduct(concrete_table, 5, "Keyboard", 75.00, 25, "Electronics", 1);
        
        // Product 6: Book, 15.99, 100, Books, in_stock=1
        insertProduct(concrete_table, 6, "Book", 15.99, 100, "Books", 1);
        
        // Product 7: Pen, 2.50, 200, Stationery, in_stock=1
        insertProduct(concrete_table, 7, "Pen", 2.50, 200, "Stationery", 1);
        
        // Product 8: Monitor, 299.99, 0, Electronics, in_stock=0
        insertProduct(concrete_table, 8, "Monitor", 299.99, 0, "Electronics", 0);
    }

    void insertProduct(Table* table, int id, const std::string& name, double price, 
                      int quantity, const std::string& category, int in_stock) {
        std::vector<Value> values;
        values.push_back(Value::createInteger(id));
        values.push_back(Value::createString(name));
        values.push_back(Value::createDouble(price));
        values.push_back(Value::createInteger(quantity));
        values.push_back(Value::createString(category));
        values.push_back(Value::createInteger(in_stock));
        
        Tuple tuple(table->getSchema(), std::move(values));
        table->insertTuple(std::move(tuple));
    }

    std::unique_ptr<hsql::SQLParserResult> parseSQL(const std::string& sql) {
        auto result = std::make_unique<hsql::SQLParserResult>();
        hsql::SQLParser::parse(sql, result.get());
        return result;
    }

    std::unique_ptr<Catalog> catalog_;
    std::unique_ptr<QueryPlanner> planner_;
    std::unique_ptr<ExecutionEngine> engine_;
};

// === NUMERIC RANGE TESTS ===

TEST_F(WhereClauseTest, PriceRangeQueries) {
    // Test expensive items
    std::string sql = "SELECT * FROM products WHERE price > 100.0";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 3);  // Laptop, Desk, Monitor
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        EXPECT_GT(tuple.getValue(2).getDouble(), 100.0f);  // price > 100
    }
}

TEST_F(WhereClauseTest, PriceBetweenRange) {
    // Test products in medium price range
    std::string sql = "SELECT * FROM products WHERE price >= 20.0 AND price <= 100.0";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 3);  // Mouse, Chair, Keyboard
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        double price = tuple.getValue(2).getDouble();
        EXPECT_GE(price, 20.0f);
        EXPECT_LE(price, 100.0f);
    }
}

TEST_F(WhereClauseTest, QuantityBasedFiltering) {
    // Test low stock items
    std::string sql = "SELECT * FROM products WHERE quantity <= 10";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 3);  // Laptop (10), Desk (5), Chair (0), Monitor (0)
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        EXPECT_LE(tuple.getValue(3).getInteger(), 10);  // quantity <= 10
    }
}

// === CATEGORY-BASED TESTS ===

TEST_F(WhereClauseTest, CategoryFiltering) {
    // Test electronics category
    std::string sql = "SELECT * FROM products WHERE category = 'Electronics'";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 4);  // Laptop, Mouse, Keyboard, Monitor
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        EXPECT_EQ(tuple.getValue(4).getString(), "Electronics");
    }
}

TEST_F(WhereClauseTest, MultiCategoryFiltering) {
    // Test multiple categories using OR
    std::string sql = "SELECT * FROM products WHERE category = 'Furniture' OR category = 'Books'";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 3);  // Desk, Chair, Book
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        std::string category = tuple.getValue(4).getString();
        EXPECT_TRUE(category == "Furniture" || category == "Books");
    }
}

// === STOCK STATUS TESTS ===

TEST_F(WhereClauseTest, InStockFiltering) {
    // Test in-stock items
    std::string sql = "SELECT * FROM products WHERE in_stock = 1";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 6);  // All except Chair and Monitor
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        EXPECT_EQ(tuple.getValue(5).getInteger(), 1);  // in_stock = 1
    }
}

TEST_F(WhereClauseTest, OutOfStockFiltering) {
    // Test out-of-stock items
    std::string sql = "SELECT * FROM products WHERE in_stock = 0";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 2);  // Chair and Monitor
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        EXPECT_EQ(tuple.getValue(5).getInteger(), 0);  // in_stock = 0
    }
}

// === COMPLEX BUSINESS LOGIC TESTS ===

TEST_F(WhereClauseTest, AvailableElectronicsQuery) {
    // Test available electronics (in_stock = 1 AND category = 'Electronics')
    std::string sql = "SELECT * FROM products WHERE in_stock = 1 AND category = 'Electronics'";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 3);  // Laptop, Mouse, Keyboard
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        EXPECT_EQ(tuple.getValue(5).getInteger(), 1);  // in_stock = 1
        EXPECT_EQ(tuple.getValue(4).getString(), "Electronics");
    }
}

TEST_F(WhereClauseTest, LowStockHighValueQuery) {
    // Test low stock but high value items
    std::string sql = "SELECT * FROM products WHERE quantity <= 10 AND price > 50.0";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 3);  // Laptop, Desk, Monitor
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        EXPECT_LE(tuple.getValue(3).getInteger(), 10);  // quantity <= 10
        EXPECT_GT(tuple.getValue(2).getDouble(), 50.0f);  // price > 50
    }
}

TEST_F(WhereClauseTest, ReorderCandidatesQuery) {
    // Test items that need reordering (quantity <= 5 OR in_stock = 0)
    std::string sql = "SELECT * FROM products WHERE quantity <= 5 OR in_stock = 0";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 3);  // Desk (5), Chair (0), Monitor (0)
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        bool low_quantity = tuple.getValue(3).getInteger() <= 5;
        bool out_of_stock = tuple.getValue(5).getInteger() == 0;
        EXPECT_TRUE(low_quantity || out_of_stock);
    }
}

// === STRING PATTERN TESTS ===

TEST_F(WhereClauseTest, NameStartsWith) {
    // Test products whose names start with specific letters
    std::string sql = "SELECT * FROM products WHERE name = 'Laptop' OR name = 'Mouse'";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 2);  // Laptop and Mouse
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        std::string name = tuple.getValue(1).getString();
        EXPECT_TRUE(name == "Laptop" || name == "Mouse");
    }
}

// === NEGATION TESTS ===

TEST_F(WhereClauseTest, NotElectronicsQuery) {
    // Test non-electronics items
    std::string sql = "SELECT * FROM products WHERE category != 'Electronics'";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 4);  // Desk, Chair, Book, Pen
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        EXPECT_NE(tuple.getValue(4).getString(), "Electronics");
    }
}

TEST_F(WhereClauseTest, NotLowPriceQuery) {
    // Test items that are not cheap (price > 20)
    std::string sql = "SELECT * FROM products WHERE price > 20.0";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 6);  // All except Book and Pen
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        EXPECT_GT(tuple.getValue(2).getDouble(), 20.0f);
    }
}

// === PROJECTION WITH WHERE TESTS ===

TEST_F(WhereClauseTest, ProjectedExpensiveItems) {
    // Test selecting specific columns for expensive items
    std::string sql = "SELECT name, price, category FROM products WHERE price > 100.0";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 3);  // Laptop, Desk, Monitor
    EXPECT_EQ(query_result->getSchema().getColumnCount(), 3);  // name, price, category
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        EXPECT_EQ(tuple.getColumnCount(), 3);  // Only projected columns
        // Note: In projected results, price is at index 1, not 2
        EXPECT_GT(tuple.getValue(1).getDouble(), 100.0f);
    }
}

TEST_F(WhereClauseTest, ProjectedStockStatus) {
    // Test selecting name and stock status for electronics
    std::string sql = "SELECT name, in_stock FROM products WHERE category = 'Electronics'";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 4);  // Laptop, Mouse, Keyboard, Monitor
    EXPECT_EQ(query_result->getSchema().getColumnCount(), 2);  // name, in_stock
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        EXPECT_EQ(tuple.getColumnCount(), 2);  // Only projected columns
    }
}

// === EDGE CASES AND BOUNDARY CONDITIONS ===

TEST_F(WhereClauseTest, ExactPriceMatch) {
    // Test exact price matching
    std::string sql = "SELECT * FROM products WHERE price = 25.50";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 1);  // Mouse
    
    const auto& tuples = query_result->getTuples();
    EXPECT_FLOAT_EQ(tuples[0].getValue(2).getDouble(), 25.50f);
    EXPECT_EQ(tuples[0].getValue(1).getString(), "Mouse");
}

TEST_F(WhereClauseTest, ZeroQuantityItems) {
    // Test items with zero quantity
    std::string sql = "SELECT * FROM products WHERE quantity = 0";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 2);  // Chair and Monitor
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        EXPECT_EQ(tuple.getValue(3).getInteger(), 0);  // quantity = 0
    }
}

TEST_F(WhereClauseTest, EmptyResultSet) {
    // Test query that returns no results
    std::string sql = "SELECT * FROM products WHERE price > 10000.0";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 0);  // No results
}

// === COMPLEX LOGICAL COMBINATIONS ===

TEST_F(WhereClauseTest, ComplexLogicalAndConditions) {
    // Test multiple AND conditions
    std::string sql = "SELECT * FROM products WHERE category = 'Electronics' AND price > 50.0 AND in_stock = 1";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 2);  // Laptop and Keyboard
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        EXPECT_EQ(tuple.getValue(4).getString(), "Electronics");
        EXPECT_GT(tuple.getValue(2).getDouble(), 50.0f);
        EXPECT_EQ(tuple.getValue(5).getInteger(), 1);
    }
}

TEST_F(WhereClauseTest, ComplexLogicalOrConditions) {
    // Test multiple OR conditions
    std::string sql = "SELECT * FROM products WHERE price < 20.0 OR quantity > 100 OR category = 'Furniture'";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 5);  // Book, Pen, Desk, Chair, Pen (quantity=200)
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        bool condition_met = tuple.getValue(2).getDouble() < 20.0f ||
                           tuple.getValue(3).getInteger() > 100 ||
                           tuple.getValue(4).getString() == "Furniture";
        EXPECT_TRUE(condition_met);
    }
}

TEST_F(WhereClauseTest, MixedAndOrConditions) {
    // Test mixed AND/OR conditions with precedence
    std::string sql = "SELECT * FROM products WHERE (category = 'Electronics' OR category = 'Books') AND price < 100.0";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 3);  // Mouse, Keyboard, Book
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        std::string category = tuple.getValue(4).getString();
        EXPECT_TRUE(category == "Electronics" || category == "Books");
        EXPECT_LT(tuple.getValue(2).getDouble(), 100.0f);
    }
}

// === COMPARISON OPERATOR EDGE CASES ===

TEST_F(WhereClauseTest, EqualityComparisons) {
    // Test exact equality matches
    std::string sql = "SELECT * FROM products WHERE price = 25.50";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 1);  // Mouse
    
    const auto& tuples = query_result->getTuples();
    EXPECT_EQ(tuples[0].getValue(1).getString(), "Mouse");
    EXPECT_EQ(tuples[0].getValue(2).getDouble(), 25.50);
}

TEST_F(WhereClauseTest, InequalityComparisons) {
    // Test not equal operator
    std::string sql = "SELECT * FROM products WHERE category != 'Electronics'";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 4);  // Desk, Chair, Book, Pen
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        EXPECT_NE(tuple.getValue(4).getString(), "Electronics");
    }
}

TEST_F(WhereClauseTest, BoundaryValueTests) {
    // Test boundary conditions
    std::string sql = "SELECT * FROM products WHERE quantity = 0";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 2);  // Chair and Monitor
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        EXPECT_EQ(tuple.getValue(3).getInteger(), 0);
    }
}

// === STRING-BASED FILTERING ===

TEST_F(WhereClauseTest, StringLengthBasedFiltering) {
    // Test filtering by string characteristics
    // Note: This uses a workaround since LIKE is not implemented
    std::string sql = "SELECT * FROM products WHERE name = 'Pen'";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 1);  // Pen
    
    const auto& tuples = query_result->getTuples();
    EXPECT_EQ(tuples[0].getValue(1).getString(), "Pen");
}

// === MULTIPLE COLUMN FILTERING ===

TEST_F(WhereClauseTest, MultiColumnComparisons) {
    // Test filtering on multiple different column types
    std::string sql = "SELECT * FROM products WHERE id >= 5 AND category = 'Electronics'";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 2);  // Keyboard and Monitor
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        EXPECT_GE(tuple.getValue(0).getInteger(), 5);
        EXPECT_EQ(tuple.getValue(4).getString(), "Electronics");
    }
}

// === EDGE CASE AND ERROR SCENARIOS ===

TEST_F(WhereClauseTest, AllRowsMatchFilter) {
    // Test query where all rows match the condition
    std::string sql = "SELECT * FROM products WHERE id > 0";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 8);  // All products
}

TEST_F(WhereClauseTest, NoRowsMatchFilter) {
    // Test query where no rows match the condition
    std::string sql = "SELECT * FROM products WHERE id < 0";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 0);  // No products
}

// === PERFORMANCE AND STRESS TESTS ===

TEST_F(WhereClauseTest, ComplexNestedConditions) {
    // Test deeply nested logical conditions
    std::string sql = "SELECT * FROM products WHERE ((category = 'Electronics' AND price > 50.0) OR (category = 'Furniture' AND in_stock = 1)) AND quantity > 5";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    // Should match: Laptop (Electronics, price>50, qty=10), Keyboard (Electronics, price>50, qty=25)
    // Should not match Desk (Furniture, in_stock=1, but only qty=5)
    EXPECT_EQ(query_result->getRowCount(), 2);
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        std::string category = tuple.getValue(4).getString();
        double price = tuple.getValue(2).getDouble();
        int in_stock = tuple.getValue(5).getInteger();
        int quantity = tuple.getValue(3).getInteger();
        
        bool condition_met = ((category == "Electronics" && price > 50.0) || 
                             (category == "Furniture" && in_stock == 1)) && 
                             quantity > 5;
        EXPECT_TRUE(condition_met);
    }
}

// === BOUNDARY VALUE TESTS ===

TEST_F(WhereClauseTest, ExactBoundaryValues) {
    // Test exact match on boundary values
    std::string sql = "SELECT * FROM products WHERE price = 999.99";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 1);  // Only Laptop
    
    const auto& tuples = query_result->getTuples();
    EXPECT_EQ(tuples[0].getValue(1).getString(), "Laptop");
}

TEST_F(WhereClauseTest, ZeroQuantityFilter) {
    // Test filtering for zero quantity items
    std::string sql = "SELECT * FROM products WHERE quantity = 0";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 2);  // Chair and Monitor
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        EXPECT_EQ(tuple.getValue(3).getInteger(), 0);  // quantity = 0
        EXPECT_EQ(tuple.getValue(5).getInteger(), 0);  // should be out of stock
    }
}

// === STRING COMPARISON TESTS ===

TEST_F(WhereClauseTest, StringEqualityTests) {
    // Test exact string matching
    std::string sql = "SELECT * FROM products WHERE name = 'Laptop'";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 1);
    EXPECT_EQ(query_result->getTuples()[0].getValue(1).getString(), "Laptop");
}

TEST_F(WhereClauseTest, StringInequalityTests) {
    // Test string inequality
    std::string sql = "SELECT * FROM products WHERE name != 'Laptop'";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 7);  // All except Laptop
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        EXPECT_NE(tuple.getValue(1).getString(), "Laptop");
    }
}

// === MULTIPLE CONDITION COMBINATIONS ===

TEST_F(WhereClauseTest, ThreeConditionAND) {
    // Test three conditions with AND
    std::string sql = "SELECT * FROM products WHERE category = 'Electronics' AND price < 100.0 AND in_stock = 1";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 2);  // Mouse and Keyboard
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        EXPECT_EQ(tuple.getValue(4).getString(), "Electronics");
        EXPECT_LT(tuple.getValue(2).getDouble(), 100.0);
        EXPECT_EQ(tuple.getValue(5).getInteger(), 1);
    }
}

TEST_F(WhereClauseTest, ThreeConditionOR) {
    // Test three conditions with OR
    std::string sql = "SELECT * FROM products WHERE category = 'Books' OR category = 'Stationery' OR price > 500.0";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 3);  // Book, Pen, Laptop
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        std::string category = tuple.getValue(4).getString();
        double price = tuple.getValue(2).getDouble();
        bool condition_met = (category == "Books" || category == "Stationery" || price > 500.0);
        EXPECT_TRUE(condition_met);
    }
}

// === EDGE CASE TESTS ===

TEST_F(WhereClauseTest, SingleRowTable) {
    // Create a table with single row and test filtering
    auto int_type = std::make_unique<IntegerType>();
    auto varchar_type = std::make_unique<VarcharType>(50);
    
    Column id_col("id", std::move(int_type));
    Column name_col("name", std::move(varchar_type));
    
    auto single_schema = std::make_unique<Schema>();
    single_schema->addColumn(std::move(id_col));
    single_schema->addColumn(std::move(name_col));
    
    catalog_->createTable("single_item", std::move(single_schema));
    
    TableBase* table = catalog_->getTable("single_item");
    Table* concrete_table = static_cast<Table*>(table);
    
    std::vector<Value> values;
    values.push_back(Value::createInteger(1));
    values.push_back(Value::createString("OnlyItem"));
    
    Tuple tuple(concrete_table->getSchema(), std::move(values));
    concrete_table->insertTuple(std::move(tuple));
    
    // Test matching condition
    std::string sql = "SELECT * FROM single_item WHERE id = 1";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 1);
}

TEST_F(WhereClauseTest, AllRowsFiltered) {
    // Test condition that filters out all rows
    std::string sql = "SELECT * FROM products WHERE price < 0.0";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 0);
}

// === NUMERICAL PRECISION TESTS ===

TEST_F(WhereClauseTest, FloatingPointPrecision) {
    // Test floating point comparisons with precise values
    std::string sql = "SELECT * FROM products WHERE price = 25.50";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 1);  // Mouse
    EXPECT_EQ(query_result->getTuples()[0].getValue(1).getString(), "Mouse");
}

TEST_F(WhereClauseTest, LargeIntegerComparison) {
    // Test with large quantity values
    std::string sql = "SELECT * FROM products WHERE quantity >= 100";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 2);  // Book (100), Pen (200)
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        EXPECT_GE(tuple.getValue(3).getInteger(), 100);
    }
}

// === BOOLEAN LOGIC STRESS TESTS ===

TEST_F(WhereClauseTest, ComplexBooleanExpression) {
    // Test complex boolean logic with mixed operators
    std::string sql = "SELECT * FROM products WHERE (category = 'Electronics' OR category = 'Furniture') AND (price > 20.0 AND price < 1000.0) AND in_stock = 1";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    // Should match: Laptop, Mouse, Keyboard, Desk
    EXPECT_EQ(query_result->getRowCount(), 4);
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
        std::string category = tuple.getValue(4).getString();
        double price = tuple.getValue(2).getDouble();
        int in_stock = tuple.getValue(5).getInteger();
        
        bool condition_met = (category == "Electronics" || category == "Furniture") &&
                           (price > 20.0 && price < 1000.0) &&
                           in_stock == 1;
        EXPECT_TRUE(condition_met);
    }
}

TEST_F(WhereClauseTest, NegationWithComplexConditions) {
    // Test NOT with complex nested conditions
    std::string sql = "SELECT * FROM products WHERE NOT (category = 'Electronics' AND price > 100.0)";
    auto result = parseSQL(sql);
    
    ASSERT_TRUE(result->isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result->getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    // Should exclude: Laptop (Electronics, price=999.99)
    // Should include: All others (7 items)
    EXPECT_EQ(query_result->getRowCount(), 7);
    
    const auto& tuples = query_result->getTuples();
    for (const auto& tuple : tuples) {
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
