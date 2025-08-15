#include "catalog/mock_catalog_builder.hpp"
#include "planner/plan_visualizer.hpp"
#include "planner/query_planner.hpp"

#include <SQLParser.h>

#include <gtest/gtest.h>

#include <fstream>
#include <iostream>

using namespace velodb;

class PlanVisualizationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create mock catalog with sample data
        catalog_ = MockCatalogBuilder::createSampleCatalog();
        planner_ = std::make_unique<QueryPlanner>(*catalog_);
    }

    void TearDown() override
    {
        // Cleanup
    }

    std::unique_ptr<Catalog> catalog_;
    std::unique_ptr<QueryPlanner> planner_;
};

TEST_F(PlanVisualizationTest, TextVisualizationSimpleSelect)
{
    std::string sql = "SELECT * FROM users";

    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);

    ASSERT_TRUE(result.isValid());
    ASSERT_EQ(result.size(), 1);

    const hsql::SQLStatement* stmt = result.getStatement(0);
    const hsql::SelectStatement* select_stmt = static_cast<const hsql::SelectStatement*>(stmt);

    auto plan = planner_->planSelect(select_stmt);
    ASSERT_NE(plan, nullptr);

    // Test text visualization
    std::string text_output = PlanVisualizer::visualizeAsText(plan);
    std::cout << "\n=== TEXT VISUALIZATION: Simple SELECT ===\n";
    std::cout << text_output << std::endl;

    EXPECT_FALSE(text_output.empty());
    EXPECT_NE(text_output.find("SeqScan"), std::string::npos);
    EXPECT_NE(text_output.find("users"), std::string::npos);
}

TEST_F(PlanVisualizationTest, TextVisualizationSelectWithWhere)
{
    std::string sql = "SELECT * FROM users WHERE id = 1";

    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);

    ASSERT_TRUE(result.isValid());
    ASSERT_EQ(result.size(), 1);

    const hsql::SQLStatement* stmt = result.getStatement(0);
    const hsql::SelectStatement* select_stmt = static_cast<const hsql::SelectStatement*>(stmt);

    auto plan = planner_->planSelect(select_stmt);
    ASSERT_NE(plan, nullptr);

    // Test text visualization
    std::string text_output = PlanVisualizer::visualizeAsText(plan);
    std::cout << "\n=== TEXT VISUALIZATION: SELECT with WHERE ===\n";
    std::cout << text_output << std::endl;

    EXPECT_FALSE(text_output.empty());
    EXPECT_NE(text_output.find("SeqScan"), std::string::npos);
}

TEST_F(PlanVisualizationTest, TextVisualizationSelectWithProjection)
{
    std::string sql = "SELECT name, email FROM users";

    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);

    ASSERT_TRUE(result.isValid());
    ASSERT_EQ(result.size(), 1);

    const hsql::SQLStatement* stmt = result.getStatement(0);
    const hsql::SelectStatement* select_stmt = static_cast<const hsql::SelectStatement*>(stmt);

    auto plan = planner_->planSelect(select_stmt);
    ASSERT_NE(plan, nullptr);

    // Test text visualization
    std::string text_output = PlanVisualizer::visualizeAsText(plan);
    std::cout << "\n=== TEXT VISUALIZATION: SELECT with Projection ===\n";
    std::cout << text_output << std::endl;

    EXPECT_FALSE(text_output.empty());
}

TEST_F(PlanVisualizationTest, GraphvizVisualizationSimpleSelect)
{
    std::string sql = "SELECT * FROM users";

    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);

    ASSERT_TRUE(result.isValid());
    ASSERT_EQ(result.size(), 1);

    const hsql::SQLStatement* stmt = result.getStatement(0);
    const hsql::SelectStatement* select_stmt = static_cast<const hsql::SelectStatement*>(stmt);

    auto plan = planner_->planSelect(select_stmt);
    ASSERT_NE(plan, nullptr);

    // Test Graphviz visualization
    std::string graphviz_output = PlanVisualizer::visualizeAsGraphviz(plan, "SimpleSelect");
    std::cout << "\n=== GRAPHVIZ VISUALIZATION: Simple SELECT ===\n";
    std::cout << graphviz_output << std::endl;

    EXPECT_FALSE(graphviz_output.empty());
    EXPECT_NE(graphviz_output.find("digraph SimpleSelect"), std::string::npos);
    EXPECT_NE(graphviz_output.find("node"), std::string::npos);
    EXPECT_NE(graphviz_output.find("diamond"), std::string::npos); // SeqScan
}

TEST_F(PlanVisualizationTest, GraphvizVisualizationComplexQuery)
{
    std::string sql = "SELECT name, email FROM users WHERE age > 25";

    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);

    ASSERT_TRUE(result.isValid());
    ASSERT_EQ(result.size(), 1);

    const hsql::SQLStatement* stmt = result.getStatement(0);
    const hsql::SelectStatement* select_stmt = static_cast<const hsql::SelectStatement*>(stmt);

    auto plan = planner_->planSelect(select_stmt);
    ASSERT_NE(plan, nullptr);

    // Test Graphviz visualization
    std::string graphviz_output = PlanVisualizer::visualizeAsGraphviz(plan, "ComplexQuery");
    std::cout << "\n=== GRAPHVIZ VISUALIZATION: Complex Query ===\n";
    std::cout << graphviz_output << std::endl;

    EXPECT_FALSE(graphviz_output.empty());
    EXPECT_NE(graphviz_output.find("digraph ComplexQuery"), std::string::npos);

    // Save to file for external visualization
    std::ofstream dot_file("complex_query.dot");
    if (dot_file.is_open()) {
        dot_file << graphviz_output;
        dot_file.close();
        std::cout << "DOT file saved as 'complex_query.dot' - use 'dot -Tpng "
                     "complex_query.dot -o complex_query.png' "
                     "to generate image\n";
    }
}

TEST_F(PlanVisualizationTest, DetailedVisualizationSimpleSelect)
{
    std::string sql = "SELECT * FROM users";

    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);

    ASSERT_TRUE(result.isValid());
    ASSERT_EQ(result.size(), 1);

    const hsql::SQLStatement* stmt = result.getStatement(0);
    const hsql::SelectStatement* select_stmt = static_cast<const hsql::SelectStatement*>(stmt);

    auto plan = planner_->planSelect(select_stmt);
    ASSERT_NE(plan, nullptr);

    // Test detailed visualization
    std::string detailed_output = PlanVisualizer::visualizeDetailed(plan);
    std::cout << "\n=== DETAILED VISUALIZATION: Simple SELECT ===\n";
    std::cout << detailed_output << std::endl;

    EXPECT_FALSE(detailed_output.empty());
    EXPECT_NE(detailed_output.find("Query Plan Analysis"), std::string::npos);
    EXPECT_NE(detailed_output.find("Level 0"), std::string::npos);
    EXPECT_NE(detailed_output.find("Output Schema"), std::string::npos);
}

TEST_F(PlanVisualizationTest, CompareAllVisualizationFormats)
{
    std::string sql = "SELECT name, age FROM users WHERE age > 21";

    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);

    ASSERT_TRUE(result.isValid());
    ASSERT_EQ(result.size(), 1);

    const hsql::SQLStatement* stmt = result.getStatement(0);
    const hsql::SelectStatement* select_stmt = static_cast<const hsql::SelectStatement*>(stmt);

    auto plan = planner_->planSelect(select_stmt);
    ASSERT_NE(plan, nullptr);

    std::cout << "\n=== COMPARISON OF ALL FORMATS ===\n";

    // Text format
    std::cout << "\n--- TEXT FORMAT ---\n";
    PlanVisualizer::printPlan(plan, std::cout, PlanVisualizer::OutputFormat::TEXT_TREE);

    // Graphviz format
    std::cout << "\n--- GRAPHVIZ FORMAT ---\n";
    PlanVisualizer::printPlan(plan, std::cout, PlanVisualizer::OutputFormat::GRAPHVIZ_DOT);

    // Detailed format
    std::cout << "\n--- DETAILED FORMAT ---\n";
    PlanVisualizer::printPlan(plan, std::cout, PlanVisualizer::OutputFormat::DETAILED);
}

TEST_F(PlanVisualizationTest, VisualizeDifferentTables)
{
    std::cout << "\n=== VISUALIZING DIFFERENT TABLES ===\n";

    // Users table
    std::string sql1 = "SELECT * FROM users";
    hsql::SQLParserResult result1;
    hsql::SQLParser::parse(sql1, &result1);
    if (result1.isValid()) {
        const hsql::SelectStatement* select_stmt = static_cast<const hsql::SelectStatement*>(result1.getStatement(0));
        auto plan = planner_->planSelect(select_stmt);
        std::cout << "\n--- USERS TABLE ---\n";
        std::cout << PlanVisualizer::visualizeAsText(plan);
    }

    // Orders table
    std::string sql2 = "SELECT * FROM orders";
    hsql::SQLParserResult result2;
    hsql::SQLParser::parse(sql2, &result2);
    if (result2.isValid()) {
        const hsql::SelectStatement* select_stmt = static_cast<const hsql::SelectStatement*>(result2.getStatement(0));
        auto plan = planner_->planSelect(select_stmt);
        std::cout << "\n--- ORDERS TABLE ---\n";
        std::cout << PlanVisualizer::visualizeAsText(plan);
    }

    // Products table
    std::string sql3 = "SELECT * FROM products";
    hsql::SQLParserResult result3;
    hsql::SQLParser::parse(sql3, &result3);
    if (result3.isValid()) {
        const hsql::SelectStatement* select_stmt = static_cast<const hsql::SelectStatement*>(result3.getStatement(0));
        auto plan = planner_->planSelect(select_stmt);
        std::cout << "\n--- PRODUCTS TABLE ---\n";
        std::cout << PlanVisualizer::visualizeAsText(plan);
    }
}

// TODO: Add tests for more complex queries when JOIN support is implemented
// TEST_F(PlanVisualizationTest, VisualizeJoinQuery) {
//     std::string sql = "SELECT u.name, o.order_date FROM users u JOIN orders o
//     ON u.id = o.user_id";
//     // Implementation pending JOIN support
// }

// TODO: Add tests for aggregation queries
// TEST_F(PlanVisualizationTest, VisualizeAggregationQuery) {
//     std::string sql = "SELECT COUNT(*) FROM users WHERE age > 25";
//     // Implementation pending aggregation support
// }
