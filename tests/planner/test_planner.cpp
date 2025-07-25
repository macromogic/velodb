#include <gtest/gtest.h>
#include "planner/planner.hpp"
#include "catalog/catalog.hpp"
#include "catalog/table.hpp"
#include "catalog/schema.hpp"
#include "types/data_type.hpp"
#include "common/exception.hpp"
#include "SQLParser.h"

using namespace velodb;

class PlannerTest : public ::testing::Test {
protected:
    void SetUp() override {
        catalog_ = std::make_unique<Catalog>();
        planner_ = std::make_unique<QueryPlanner>(*catalog_);
        
        auto schema = std::make_unique<Schema>();
        schema->addColumnInfo({ "id", std::make_unique<IntegerType>() });
        schema->addColumnInfo({ "name", std::make_unique<VarcharType>(100) });
        
        catalog_->createTable("users", std::move(schema));
    }

    void TearDown() override {
        // Cleanup code if needed
    }

    std::unique_ptr<Catalog> catalog_;
    std::unique_ptr<QueryPlanner> planner_;
};

TEST_F(PlannerTest, CreateQueryPlanner) {
    EXPECT_NE(planner_, nullptr);
}

TEST_F(PlannerTest, PlanSimpleSelect) {
    std::string sql = "SELECT * FROM users";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    ASSERT_EQ(result.size(), 1);
    
    const hsql::SQLStatement* stmt = result.getStatement(0);
    ASSERT_EQ(stmt->type(), hsql::kStmtSelect);
    
    const hsql::SelectStatement* select_stmt = static_cast<const hsql::SelectStatement*>(stmt);
    
    auto plan = planner_->planSelect(select_stmt);
    ASSERT_NE(plan, nullptr);
    
    // Should create a ScanFilter plan node
    EXPECT_EQ(plan->getPlanType(), PlanType::SCAN_FILTER);
    EXPECT_EQ(plan->getOutputSchema().getColumnCount(), 2);
}

TEST_F(PlannerTest, PlanSelectWithWhere) {
    std::string sql = "SELECT * FROM users WHERE id = 1";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    ASSERT_EQ(result.size(), 1);
    
    const hsql::SQLStatement* stmt = result.getStatement(0);
    const hsql::SelectStatement* select_stmt = static_cast<const hsql::SelectStatement*>(stmt);

    auto plan = planner_->planSelect(select_stmt);
    ASSERT_NE(plan, nullptr);
    
    // Should still be a ScanFilter (predicate pushed down)
    EXPECT_EQ(plan->getPlanType(), PlanType::SCAN_FILTER);
}

TEST_F(PlannerTest, PlanSelectWithProjection) {
    std::string sql = "SELECT id FROM users";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    ASSERT_EQ(result.size(), 1);
    
    const hsql::SQLStatement* stmt = result.getStatement(0);
    const hsql::SelectStatement* select_stmt = static_cast<const hsql::SelectStatement*>(stmt);

    auto plan = planner_->planSelect(select_stmt);
    ASSERT_NE(plan, nullptr);
    
    // Should create a plan with projection
    EXPECT_NE(plan->getPlanType(), PlanType::INVALID);
}

TEST_F(PlannerTest, PlanInvalidTable) {
    std::string sql = "SELECT * FROM nonexistent_table";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    ASSERT_EQ(result.size(), 1);
    
    const hsql::SQLStatement* stmt = result.getStatement(0);
    const hsql::SelectStatement* select_stmt = static_cast<const hsql::SelectStatement*>(stmt);
    
    // Should throw an exception for non-existent table
    EXPECT_THROW(planner_->planSelect(select_stmt), CatalogError);
}

TEST_F(PlannerTest, PlanComplexWhere) {
    std::string sql = "SELECT * FROM users WHERE id > 0 AND name = 'Alice'";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    ASSERT_EQ(result.size(), 1);
    
    const hsql::SQLStatement* stmt = result.getStatement(0);
    const hsql::SelectStatement* select_stmt = static_cast<const hsql::SelectStatement*>(stmt);

    auto plan = planner_->planSelect(select_stmt);
    ASSERT_NE(plan, nullptr);
    
    // Should create a valid plan with complex predicate
    EXPECT_NE(plan->getPlanType(), PlanType::INVALID);
}

// TODO: Add more comprehensive planner tests when additional features are implemented
// - JOIN planning tests
// - Subquery planning tests
// - Aggregation planning tests
// - ORDER BY planning tests
// - LIMIT planning tests
// - Complex expression planning tests
