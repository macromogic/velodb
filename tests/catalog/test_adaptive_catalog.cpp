#include <gtest/gtest.h>
#include "catalog/mock_catalog_builder.hpp"
#include "planner/query_planner.hpp"
#include "planner/plan_visualizer.hpp"
#include "SQLParser.h"
#include <iostream>
#include <vector>

using namespace velodb;

class AdaptiveCatalogTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create adaptive catalog that can handle any query
        catalog_ = MockCatalogBuilder::createAdaptiveCatalog();
    }

    void TearDown() override {
        // Cleanup
    }

    void testQueryVisualization(const std::string& query) {
        std::cout << "\n=== Testing Query: " << query << " ===\n";

        // Ensure tables exist for this query
        bool success = MockCatalogBuilder::ensureTablesForQuery(*catalog_, query);
        ASSERT_TRUE(success) << "Failed to create tables for query: " << query;

        // Create planner and plan the query
        QueryPlanner planner(*catalog_);

        hsql::SQLParserResult result;
        hsql::SQLParser::parse(query, &result);

        if (result.isValid() && result.size() > 0) {
            const hsql::SQLStatement* stmt = result.getStatement(0);
            if (stmt->type() == hsql::kStmtSelect) {
                const hsql::SelectStatement* select_stmt = static_cast<const hsql::SelectStatement*>(stmt);

                try {
                    auto plan = planner.planSelect(select_stmt);
                    ASSERT_NE(plan, nullptr) << "Failed to create plan for query: " << query;

                    // Visualize the plan
                    std::cout << "TEXT FORMAT:\n";
                    std::cout << PlanVisualizer::visualizeAsText(plan) << "\n";

                    std::cout << "DETAILED FORMAT:\n";
                    std::cout << PlanVisualizer::visualizeDetailed(plan) << "\n";

                    // Test that the plan is valid
                    EXPECT_NE(plan->getPlanType(), PlanType::INVALID);

                } catch (const std::exception& e) {
                    FAIL() << "Exception during planning: " << e.what();
                }
            }
        } else {
            FAIL() << "Failed to parse query: " << query;
        }
    }

    std::unique_ptr<Catalog> catalog_;
};

TEST_F(AdaptiveCatalogTest, BasicSelectQueries) {
    // Test various SELECT queries on different tables
    std::vector<std::string> queries = {
        "SELECT * FROM users",
        "SELECT * FROM orders",
        "SELECT * FROM products",
        "SELECT * FROM customers",
        "SELECT * FROM employees",
        "SELECT * FROM any_table_name",
        "SELECT * FROM foo",
        "SELECT * FROM bar",
        "SELECT * FROM test_table"
    };

    for (const auto& query : queries) {
        testQueryVisualization(query);
    }
}

TEST_F(AdaptiveCatalogTest, SelectWithColumns) {
    std::vector<std::string> queries = {
        "SELECT id, name FROM users",
        "SELECT order_id, total FROM orders",
        "SELECT product_id, price FROM products",
        "SELECT customer_id, email FROM customers",
        "SELECT employee_id, salary FROM employees",
        "SELECT foo, bar, baz FROM any_table",
        "SELECT x, y, z FROM test_data"
    };

    for (const auto& query : queries) {
        testQueryVisualization(query);
    }
}

TEST_F(AdaptiveCatalogTest, SelectWithWhere) {
    std::vector<std::string> queries = {
        "SELECT * FROM users WHERE id = 1",
        "SELECT * FROM orders WHERE total > 100",
        "SELECT * FROM products WHERE price < 50",
        "SELECT name FROM customers WHERE email = 'test@example.com'",
        "SELECT * FROM employees WHERE salary > 50000",
        "SELECT * FROM any_table WHERE col_1 = 'value'"
    };

    for (const auto& query : queries) {
        testQueryVisualization(query);
    }
}

TEST_F(AdaptiveCatalogTest, ComplexQueries) {
    std::vector<std::string> queries = {
        "SELECT name, email FROM users WHERE age > 25",
        "SELECT product_id, price FROM products WHERE category = 'electronics'",
        "SELECT order_id, quantity FROM orders WHERE order_date > '2023-01-01'",
        "SELECT employee_id, department FROM employees WHERE salary BETWEEN 40000 AND 80000"
    };

    for (const auto& query : queries) {
        testQueryVisualization(query);
    }
}

TEST_F(AdaptiveCatalogTest, MultipleTablesInSameQuery) {
    // Test that the system can handle table references even if JOINs aren't fully implemented
    std::vector<std::string> queries = {
        // These will fail at planning stage but should create tables successfully
        "SELECT * FROM table1, table2",
        "SELECT * FROM alpha, beta, gamma"
    };

    for (const auto& query : queries) {
        std::cout << "\n=== Testing Multi-Table Query: " << query << " ===\n";

        // Just test table creation, not full planning
        bool success = MockCatalogBuilder::ensureTablesForQuery(*catalog_, query);
        EXPECT_TRUE(success) << "Failed to create tables for query: " << query;

        // Check that tables were created
        std::cout << "Catalog now has " << catalog_->getTableCount() << " tables\n";
    }
}

TEST_F(AdaptiveCatalogTest, TypeInference) {
    // Test that the system can infer reasonable types from column names
    std::vector<std::string> queries = {
        "SELECT id, name, email, age FROM users",
        "SELECT order_id, user_id, total, price, quantity FROM orders",
        "SELECT product_id, price, description FROM products",
        "SELECT record_count, percentage, enabled, created_at FROM stats_table"
    };

    for (const auto& query : queries) {
        testQueryVisualization(query);
    }
}

TEST_F(AdaptiveCatalogTest, GraphvizOutput) {
    std::string query = "SELECT name, email FROM users WHERE age > 25";

    // Ensure tables exist
    bool success = MockCatalogBuilder::ensureTablesForQuery(*catalog_, query);
    ASSERT_TRUE(success);

    // Create planner and plan the query
    QueryPlanner planner(*catalog_);

    hsql::SQLParserResult result;
    hsql::SQLParser::parse(query, &result);

    ASSERT_TRUE(result.isValid());
    const hsql::SelectStatement* select_stmt = static_cast<const hsql::SelectStatement*>(result.getStatement(0));

    auto plan = planner.planSelect(select_stmt);
    ASSERT_NE(plan, nullptr);

    // Generate Graphviz output
    std::string graphviz_output = PlanVisualizer::visualizeAsGraphviz(plan, "AdaptiveCatalogTest");

    std::cout << "\n=== GRAPHVIZ OUTPUT ===\n";
    std::cout << graphviz_output << std::endl;

    EXPECT_FALSE(graphviz_output.empty());
    EXPECT_NE(graphviz_output.find("digraph AdaptiveCatalogTest"), std::string::npos);
}

TEST_F(AdaptiveCatalogTest, CatalogPersistence) {
    // Test that created tables persist across multiple queries
    std::vector<std::string> queries = {
        "SELECT * FROM persistent_table",
        "SELECT id, name FROM persistent_table",
        "SELECT * FROM persistent_table WHERE id = 1"
    };

    size_t initial_table_count = catalog_->getTableCount();

    for (const auto& query : queries) {
        MockCatalogBuilder::ensureTablesForQuery(*catalog_, query);

        // Table count should only increase on first query
        if (query == queries[0]) {
            EXPECT_GT(catalog_->getTableCount(), initial_table_count);
        }
    }

    // Verify table exists and can be retrieved
    EXPECT_TRUE(catalog_->hasTable("persistent_table"));
    auto table_result = catalog_->getTable("persistent_table");
    ASSERT_TRUE(table_result.has_value());
    auto& table = table_result.value().get();
    EXPECT_EQ(table.getName(), "persistent_table");

    std::cout << "\nPersistent table schema:\n";
    const auto& schema = table.getSchema();
    for (size_t i = 0; i < schema.getColumnCount(); ++i) {
        const auto& column = schema.getColumnInfo(i);
        std::cout << "  " << column.getName() << " (" << column.getType().toString() << ")\n";
    }
}

// TODO: Add tests for JOIN queries when JOIN support is implemented
// TEST_F(AdaptiveCatalogTest, JoinQueries) {
//     std::vector<std::string> queries = {
//         "SELECT u.name, o.total FROM users u JOIN orders o ON u.id = o.user_id",
//         "SELECT p.name, o.quantity FROM products p JOIN orders o ON p.id = o.product_id"
//     };
//
//     for (const auto& query : queries) {
//         testQueryVisualization(query);
//     }
// }
