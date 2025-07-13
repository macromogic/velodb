#pragma once

// Core type system
#include "types/data_type.hpp"
#include "types/value.hpp"

// Catalog system
#include "catalog/catalog.hpp"
#include "catalog/schema.hpp"
#include "catalog/table.hpp"

// Execution system
#include "execution/execution_engine.hpp"
#include "execution/expression.hpp"
#include "execution/operator.hpp"

// Query planning
#include "planner.hpp"

// Version information (generated from CMakeLists.txt)
#include "velodb_version.hpp"

// SQL Parser integration
#include "SQLParser.h"
#include "util/sqlhelper.h"

namespace velodb {

// Main database class that ties everything together
class Database {
public:
    Database();
    ~Database() = default;

    // Delete copy constructor and assignment
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Database operations
    bool initialize();
    void shutdown();

    // Table management
    bool createTable(const std::string& table_name, std::unique_ptr<Schema> schema);
    bool dropTable(const std::string& table_name);
    [[nodiscard]] bool hasTable(const std::string& table_name) const;
    [[nodiscard]] Table* getTable(const std::string& table_name) const;

    // Data manipulation
    bool insertTuple(const std::string& table_name, const Tuple& tuple);
    bool insertTuple(const std::string& table_name, Tuple&& tuple);

    // Query execution
    std::unique_ptr<QueryResult> executeQuery(const std::string& sql);

    // Statistics
    [[nodiscard]] size_t getTableCount() const;
    [[nodiscard]] std::vector<std::string> getTableNames() const;
    [[nodiscard]] std::string getDatabaseInfo() const;

private:
    std::unique_ptr<Catalog> catalog_;
    std::unique_ptr<ExecutionEngine> execution_engine_;
    bool initialized_ { false };
};

namespace util {

    // Create common data types
    std::unique_ptr<DataType> createIntegerType();
    std::unique_ptr<DataType> createBigIntType();
    std::unique_ptr<DataType> createDoubleType();
    std::unique_ptr<DataType> createBooleanType();
    std::unique_ptr<DataType> createVarcharType(size_t max_length);

    // Create common values
    Value createIntegerValue(int32_t value);
    Value createBigIntValue(int64_t value);
    Value createDoubleValue(double value);
    Value createBooleanValue(bool value);
    Value createStringValue(const std::string& value);
    Value createNullValue(DataTypeId type_id);

    // Schema builder helpers
    std::unique_ptr<Schema> createSchema(std::vector<Column> columns);
    Column createColumn(const std::string& name, std::unique_ptr<DataType> type, bool nullable = true);

    // Sample data creation for testing
    std::unique_ptr<Database> createSampleDatabase();
    void populateSampleData(Database* db);

} // namespace util

} // namespace velodb
