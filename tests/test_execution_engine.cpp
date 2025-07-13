#include <gtest/gtest.h>
#include "execution/execution_engine.hpp"
#include "planner.hpp"
#include "catalog/catalog.hpp"
#include "catalog/table.hpp"
#include "catalog/schema.hpp"
#include "types/data_type.hpp"
#include "SQLParser.h"

using namespace velodb;

class ExecutionEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        catalog_ = std::make_unique<Catalog>();
        planner_ = std::make_unique<QueryPlanner>(catalog_.get());
        engine_ = std::make_unique<ExecutionEngine>(catalog_.get());
        
        // Create a test table with data
        auto int_type = std::make_unique<IntegerType>();
        auto varchar_type = std::make_unique<VarcharType>(100);
        
        Column id_col("id", std::move(int_type));
        Column name_col("name", std::move(varchar_type));
        
        auto schema = std::make_unique<Schema>();
        schema->addColumn(std::move(id_col));
        schema->addColumn(std::move(name_col));
        
        catalog_->createTable("users", std::move(schema));
        
        // Insert test data
        TableBase* table = catalog_->getTable("users");
        Table* concrete_table = static_cast<Table*>(table);
        
        std::vector<Value> values1;
        values1.push_back(Value::createInteger(1));
        values1.push_back(Value::createString("Alice"));
        Tuple tuple1(table->getSchema(), std::move(values1));
        concrete_table->insertTuple(std::move(tuple1));
        
        std::vector<Value> values2;
        values2.push_back(Value::createInteger(2));
        values2.push_back(Value::createString("Bob"));
        Tuple tuple2(table->getSchema(), std::move(values2));
        concrete_table->insertTuple(std::move(tuple2));
    }

    void TearDown() override {
        // Cleanup code if needed
    }

    std::unique_ptr<Catalog> catalog_;
    std::unique_ptr<QueryPlanner> planner_;
    std::unique_ptr<ExecutionEngine> engine_;
};

TEST_F(ExecutionEngineTest, CreateExecutionEngine) {
    EXPECT_NE(engine_, nullptr);
}

TEST_F(ExecutionEngineTest, ExecuteSimpleSelect) {
    std::string sql = "SELECT * FROM users";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    ASSERT_EQ(result.size(), 1);
    
    const hsql::SQLStatement* stmt = result.getStatement(0);
    const hsql::SelectStatement* select_stmt = static_cast<const hsql::SelectStatement*>(stmt);
    
    auto plan = planner_->planSelect(select_stmt);
    ASSERT_NE(plan, nullptr);

    auto query_result = engine_->executePlan(std::move(plan));
    ASSERT_NE(query_result, nullptr);
    
    // Should return 2 rows
    EXPECT_EQ(query_result->getRowCount(), 2);
    EXPECT_EQ(query_result->getSchema().getColumnCount(), 2);
}

TEST_F(ExecutionEngineTest, ExecuteSelectWithWhere) {
    std::string sql = "SELECT * FROM users WHERE id = 1";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result.getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    
    // Should return 1 row (Alice)
    EXPECT_EQ(query_result->getRowCount(), 1);

    const auto& tuples = query_result->getTuples();
    EXPECT_EQ(tuples[0].getValue(0).getInteger(), 1);
    EXPECT_EQ(tuples[0].getValue(1).getString(), "Alice");
}

TEST_F(ExecutionEngineTest, ExecuteSelectWithProjection) {
    std::string sql = "SELECT name FROM users";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result.getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    
    // Should return 2 rows with 1 column each
    EXPECT_EQ(query_result->getRowCount(), 2);
    EXPECT_EQ(query_result->getSchema().getColumnCount(), 1);
    
    // Check that we only get the name column
    const auto& tuples = query_result->getTuples();
    EXPECT_EQ(tuples[0].getColumnCount(), 1);
    EXPECT_EQ(tuples[1].getColumnCount(), 1);
}

TEST_F(ExecutionEngineTest, ExecuteInvalidQuery) {
    std::string sql = "SELECT * FROM nonexistent_table";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result.getStatement(0));
    
    // Planning should fail for non-existent table
    EXPECT_THROW(planner_->planSelect(select_stmt), std::runtime_error);
}

TEST_F(ExecutionEngineTest, ExecuteEmptyResult) {
    std::string sql = "SELECT * FROM users WHERE id = 999";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result.getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    
    // Should return 0 rows
    EXPECT_EQ(query_result->getRowCount(), 0);
}

// TODO: Add more comprehensive execution engine tests when additional features are implemented
// - JOIN execution tests
// - Aggregation execution tests
// - ORDER BY execution tests
// - LIMIT execution tests
// - Complex predicate execution tests
// - Performance tests
