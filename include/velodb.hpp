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
#include "expression/expression.hpp"
#include "operator/operator.hpp"

// Query planning
#include "planner/planner.hpp"

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
    bool hasTable(const std::string& table_name) const;
    std::optional<std::reference_wrapper<Table>> getTable(const std::string& table_name) const;

    // Data manipulation
    bool insertTuple(const std::string& table_name, const Tuple& tuple);
    bool insertTuple(const std::string& table_name, Tuple&& tuple);

    // Query execution
    Result<View> executeQuery(const std::string& sql);

    // Statistics
    size_t getTableCount() const;
    std::vector<std::string> getTableNames() const;
    std::string getDatabaseInfo() const;

private:
    std::unique_ptr<Catalog> catalog_;
    std::unique_ptr<ExecutionEngine> execution_engine_;
    bool initialized_ { false };
};

} // namespace velodb
