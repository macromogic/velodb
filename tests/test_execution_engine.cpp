#include <gtest/gtest.h>
#include "execution/execution_engine.hpp"
#include "planner.hpp"
#include "catalog/catalog.hpp"
#include "catalog/table.hpp"
#include "catalog/schema.hpp"
#include "types/data_type.hpp"
#include "common/traced_exception.hpp"
#include "SQLParser.h"

using namespace velodb;

// Helper function to convert QueryResult to vector of tuples for testing
static std::vector<Tuple> queryResultToTuples(const QueryResult& result) {
    std::vector<Tuple> tuples;
    const size_t row_count = result.getRowCount();
    const Schema& schema = result.getSchema();
    
    for (size_t row_id = 0; row_id < row_count; ++row_id) {
        std::vector<Value> values;
        values.reserve(schema.getColumnCount());
        for (size_t col_idx = 0; col_idx < schema.getColumnCount(); ++col_idx) {
            values.push_back(result.getValue(row_id, col_idx));
        }
        tuples.emplace_back(schema, std::move(values));
    }
    
    return tuples;
}

class ExecutionEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        catalog_ = std::make_unique<Catalog>();
        planner_ = std::make_unique<QueryPlanner>(catalog_.get());
        engine_ = std::make_unique<ExecutionEngine>(catalog_.get());
        
        // Create a test table with more diverse data types
        auto int_type = std::make_unique<IntegerType>();
        auto varchar_type = std::make_unique<VarcharType>(100);
        auto double_type = std::make_unique<DoubleType>();
        
        Column id_col("id", std::move(int_type));
        Column name_col("name", std::move(varchar_type));
        Column age_col("age", std::make_unique<IntegerType>());
        Column salary_col("salary", std::move(double_type));
        Column department_col("department", std::make_unique<VarcharType>(50));
        
        auto schema = std::make_unique<Schema>();
        schema->addColumn(std::move(id_col));
        schema->addColumn(std::move(name_col));
        schema->addColumn(std::move(age_col));
        schema->addColumn(std::move(salary_col));
        schema->addColumn(std::move(department_col));
        
        catalog_->createTable("employees", std::move(schema));
        
        // Insert more diverse test data
        TableBase* table = catalog_->getTable("employees");
        Table* concrete_table = static_cast<Table*>(table);
        
        // Employee 1: Alice, 25, 50000.0, Engineering
        std::vector<Value> values1;
        values1.push_back(Value::createInteger(1));
        values1.push_back(Value::createString("Alice"));
        values1.push_back(Value::createInteger(25));
        values1.push_back(Value::createDouble(50000.0));
        values1.push_back(Value::createString("Engineering"));
        concrete_table->insertRow(values1);
        
        // Employee 2: Bob, 30, 60000.0, Sales
        std::vector<Value> values2;
        values2.push_back(Value::createInteger(2));
        values2.push_back(Value::createString("Bob"));
        values2.push_back(Value::createInteger(30));
        values2.push_back(Value::createDouble(60000.0));
        values2.push_back(Value::createString("Sales"));
        concrete_table->insertRow(values2);
        
        // Employee 3: Charlie, 35, 75000.0, Engineering
        std::vector<Value> values3;
        values3.push_back(Value::createInteger(3));
        values3.push_back(Value::createString("Charlie"));
        values3.push_back(Value::createInteger(35));
        values3.push_back(Value::createDouble(75000.0));
        values3.push_back(Value::createString("Engineering"));
        concrete_table->insertRow(values3);
        
        // Employee 4: Diana, 28, 45000.0, HR
        std::vector<Value> values4;
        values4.push_back(Value::createInteger(4));
        values4.push_back(Value::createString("Diana"));
        values4.push_back(Value::createInteger(28));
        values4.push_back(Value::createDouble(45000.0));
        values4.push_back(Value::createString("HR"));
        concrete_table->insertRow(values4);
        
        // Employee 5: Eve, 32, 55000.0, Sales
        std::vector<Value> values5;
        values5.push_back(Value::createInteger(5));
        values5.push_back(Value::createString("Eve"));
        values5.push_back(Value::createInteger(32));
        values5.push_back(Value::createDouble(55000.0));
        values5.push_back(Value::createString("Sales"));
        concrete_table->insertRow(values5);
        
        // Create a legacy test table for backward compatibility
        auto legacy_int_type = std::make_unique<IntegerType>();
        auto legacy_varchar_type = std::make_unique<VarcharType>(100);
        
        Column legacy_id_col("id", std::move(legacy_int_type));
        Column legacy_name_col("name", std::move(legacy_varchar_type));
        
        auto legacy_schema = std::make_unique<Schema>();
        legacy_schema->addColumn(std::move(legacy_id_col));
        legacy_schema->addColumn(std::move(legacy_name_col));
        
        catalog_->createTable("users", std::move(legacy_schema));
        
        // Insert legacy test data
        TableBase* legacy_table = catalog_->getTable("users");
        Table* legacy_concrete_table = static_cast<Table*>(legacy_table);
        
        std::vector<Value> legacy_values1;
        legacy_values1.push_back(Value::createInteger(1));
        legacy_values1.push_back(Value::createString("Alice"));
        legacy_concrete_table->insertRow(legacy_values1);
        
        std::vector<Value> legacy_values2;
        legacy_values2.push_back(Value::createInteger(2));
        legacy_values2.push_back(Value::createString("Bob"));
        legacy_concrete_table->insertRow(legacy_values2);
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

    const auto tuples = queryResultToTuples(*query_result);
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
    const auto tuples = queryResultToTuples(*query_result);
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
    EXPECT_THROW(planner_->planSelect(select_stmt), CatalogError);
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

// === INTEGER COMPARISON TESTS ===

TEST_F(ExecutionEngineTest, WhereIntegerEquals) {
    std::string sql = "SELECT * FROM employees WHERE id = 3";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result.getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 1);
    
    const auto tuples = queryResultToTuples(*query_result);
    EXPECT_EQ(tuples[0].getValue(0).getInteger(), 3);
    EXPECT_EQ(tuples[0].getValue(1).getString(), "Charlie");
}

TEST_F(ExecutionEngineTest, WhereIntegerNotEquals) {
    std::string sql = "SELECT * FROM employees WHERE id != 1";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result.getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 4);  // All except Alice
    
    const auto tuples = queryResultToTuples(*query_result);
    // Should not contain Alice (id=1)
    for (const auto& tuple : tuples) {
        EXPECT_NE(tuple.getValue(0).getInteger(), 1);
    }
}

TEST_F(ExecutionEngineTest, WhereIntegerGreaterThan) {
    std::string sql = "SELECT * FROM employees WHERE age > 30";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result.getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 2);  // Charlie (35) and Eve (32)
    
    const auto tuples = queryResultToTuples(*query_result);
    for (const auto& tuple : tuples) {
        EXPECT_GT(tuple.getValue(2).getInteger(), 30);  // age column
    }
}

TEST_F(ExecutionEngineTest, WhereIntegerGreaterThanOrEqual) {
    std::string sql = "SELECT * FROM employees WHERE age >= 30";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result.getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 3);  // Bob (30), Charlie (35), Eve (32)
    
    const auto tuples = queryResultToTuples(*query_result);
    for (const auto& tuple : tuples) {
        EXPECT_GE(tuple.getValue(2).getInteger(), 30);  // age column
    }
}

TEST_F(ExecutionEngineTest, WhereIntegerLessThan) {
    std::string sql = "SELECT * FROM employees WHERE age < 30";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result.getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 2);  // Alice (25) and Diana (28)
    
    const auto tuples = queryResultToTuples(*query_result);
    for (const auto& tuple : tuples) {
        EXPECT_LT(tuple.getValue(2).getInteger(), 30);  // age column
    }
}

TEST_F(ExecutionEngineTest, WhereIntegerLessThanOrEqual) {
    std::string sql = "SELECT * FROM employees WHERE age <= 28";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result.getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 2);  // Alice (25) and Diana (28)
    
    const auto tuples = queryResultToTuples(*query_result);
    for (const auto& tuple : tuples) {
        EXPECT_LE(tuple.getValue(2).getInteger(), 28);  // age column
    }
}

// === FLOAT COMPARISON TESTS ===

TEST_F(ExecutionEngineTest, WhereFloatEquals) {
    std::string sql = "SELECT * FROM employees WHERE salary = 60000.0";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result.getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 1);  // Bob
    
    const auto tuples = queryResultToTuples(*query_result);
    EXPECT_FLOAT_EQ(tuples[0].getValue(3).getDouble(), 60000.0f);  // salary column
    EXPECT_EQ(tuples[0].getValue(1).getString(), "Bob");
}

TEST_F(ExecutionEngineTest, WhereFloatGreaterThan) {
    std::string sql = "SELECT * FROM employees WHERE salary > 55000.0";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result.getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 2);  // Bob (60000) and Charlie (75000)
    
    const auto tuples = queryResultToTuples(*query_result);
    for (const auto& tuple : tuples) {
        EXPECT_GT(tuple.getValue(3).getDouble(), 55000.0f);  // salary column
    }
}

TEST_F(ExecutionEngineTest, WhereFloatLessThan) {
    std::string sql = "SELECT * FROM employees WHERE salary < 50000.0";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result.getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 1);  // Diana (45000)
    
    const auto tuples = queryResultToTuples(*query_result);
    EXPECT_LT(tuples[0].getValue(3).getDouble(), 50000.0f);  // salary column
    EXPECT_EQ(tuples[0].getValue(1).getString(), "Diana");
}

// === STRING COMPARISON TESTS ===

TEST_F(ExecutionEngineTest, WhereStringEquals) {
    std::string sql = "SELECT * FROM employees WHERE name = 'Alice'";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result.getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 1);
    
    const auto tuples = queryResultToTuples(*query_result);
    EXPECT_EQ(tuples[0].getValue(1).getString(), "Alice");
    EXPECT_EQ(tuples[0].getValue(0).getInteger(), 1);
}

TEST_F(ExecutionEngineTest, WhereStringNotEquals) {
    std::string sql = "SELECT * FROM employees WHERE department != 'Engineering'";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result.getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 3);  // Bob (Sales), Diana (HR), Eve (Sales)
    
    const auto tuples = queryResultToTuples(*query_result);
    for (const auto& tuple : tuples) {
        EXPECT_NE(tuple.getValue(4).getString(), "Engineering");  // department column
    }
}

TEST_F(ExecutionEngineTest, WhereDepartmentEquals) {
    std::string sql = "SELECT * FROM employees WHERE department = 'Sales'";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result.getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 2);  // Bob and Eve
    
    const auto tuples = queryResultToTuples(*query_result);
    for (const auto& tuple : tuples) {
        EXPECT_EQ(tuple.getValue(4).getString(), "Sales");  // department column
    }
}

// === LOGICAL OPERATOR TESTS ===

TEST_F(ExecutionEngineTest, WhereLogicalAnd) {
    std::string sql = "SELECT * FROM employees WHERE department = 'Engineering' AND age > 30";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result.getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 1);  // Charlie (Engineering, 35)
    
    const auto tuples = queryResultToTuples(*query_result);
    EXPECT_EQ(tuples[0].getValue(4).getString(), "Engineering");  // department
    EXPECT_GT(tuples[0].getValue(2).getInteger(), 30);  // age > 30
    EXPECT_EQ(tuples[0].getValue(1).getString(), "Charlie");
}

TEST_F(ExecutionEngineTest, WhereLogicalOr) {
    std::string sql = "SELECT * FROM employees WHERE age < 26 OR salary > 70000";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result.getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 2);  // Alice (25) and Charlie (75000)
    
    const auto tuples = queryResultToTuples(*query_result);
    for (const auto& tuple : tuples) {
        bool age_condition = tuple.getValue(2).getInteger() < 26;
        bool salary_condition = tuple.getValue(3).getDouble() > 70000.0f;
        EXPECT_TRUE(age_condition || salary_condition);
    }
}

TEST_F(ExecutionEngineTest, WhereComplexLogical) {
    std::string sql = "SELECT * FROM employees WHERE (department = 'Sales' OR department = 'HR') AND age >= 28";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result.getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 3);  // Bob (Sales, 30), Diana (HR, 28), Eve (Sales, 32)
    
    const auto tuples = queryResultToTuples(*query_result);
    for (const auto& tuple : tuples) {
        std::string dept = tuple.getValue(4).getString();
        int age = tuple.getValue(2).getInteger();
        EXPECT_TRUE((dept == "Sales" || dept == "HR") && age >= 28);
    }
}

// === EDGE CASE TESTS ===

TEST_F(ExecutionEngineTest, WhereNoMatches) {
    std::string sql = "SELECT * FROM employees WHERE age > 100";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result.getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 0);  // No employees over 100
}

TEST_F(ExecutionEngineTest, WhereAllMatch) {
    std::string sql = "SELECT * FROM employees WHERE age > 0";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result.getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 5);  // All employees
}

TEST_F(ExecutionEngineTest, WhereWithProjection) {
    std::string sql = "SELECT name, department FROM employees WHERE salary >= 55000";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result.getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 3);  // Bob, Charlie, Eve
    EXPECT_EQ(query_result->getSchema().getColumnCount(), 2);  // name, department only
    
    const auto tuples = queryResultToTuples(*query_result);
    for (const auto& tuple : tuples) {
        EXPECT_EQ(tuple.getColumnCount(), 2);  // Only name and department
    }
}

// === MIXED TYPE COMPARISON TESTS ===

TEST_F(ExecutionEngineTest, WhereMixedConditions) {
    std::string sql = "SELECT * FROM employees WHERE id <= 3 AND salary > 50000 AND department != 'HR'";
    
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);
    
    ASSERT_TRUE(result.isValid());
    const hsql::SelectStatement* select_stmt = 
        static_cast<const hsql::SelectStatement*>(result.getStatement(0));

    auto plan = planner_->planSelect(select_stmt);
    auto query_result = engine_->executePlan(std::move(plan));

    ASSERT_NE(query_result, nullptr);
    EXPECT_EQ(query_result->getRowCount(), 2);  // Bob (id=2, salary=60000, Sales) and Charlie (id=3, salary=75000, Engineering)
    
    const auto tuples = queryResultToTuples(*query_result);
    for (const auto& tuple : tuples) {
        EXPECT_LE(tuple.getValue(0).getInteger(), 3);  // id <= 3
        EXPECT_GT(tuple.getValue(3).getDouble(), 50000.0f);  // salary > 50000
        EXPECT_NE(tuple.getValue(4).getString(), "HR");  // department != 'HR'
    }
}

// TODO: Add more comprehensive execution engine tests when additional features are implemented
// - JOIN execution tests
// - Aggregation execution tests
// - ORDER BY execution tests
// - LIMIT execution tests
// - NULL value handling tests
// - Performance tests
// - Error handling tests for invalid predicates
