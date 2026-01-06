#pragma once

#include "common/copy_traits.hpp"

// Core type system
#include "data/data_type.hpp"
#include "data/value.hpp"

// Catalog system
#include "catalog/catalog.hpp"
#include "catalog/schema.hpp"
#include "catalog/table.hpp"

// Execution system
#include "execution/execution_engine.hpp"
#include "expression/expression.hpp"
#include "operator/operator.hpp"

// Query planning
#include "planner/query_planner.hpp"

// Version information (generated from CMakeLists.txt)
#include "velodb_version.hpp"

// SQL Parser integration
#include <SQLParser.h>
#include <util/sqlhelper.h>

namespace velodb {

// Main database class that ties everything together
class Database : private NonCopyable {
public:
    Database();
    ~Database() = default;

    // Add move constructor and assignment
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;

    // Database operations
    void initialize();
    void shutdown();

    // Table management
    bool createTable(const std::string& table_name, Schema schema);
    bool hasTable(const std::string& table_name) const;
    std::optional<std::reference_wrapper<const Table>> getTable(const std::string& table_name) const;

    // Query execution
    Result<QueryResult> executeQuery(const std::string& sql, QueryStatistics* stats = nullptr);

    // Catalog access (temporary for benchmarking)
    Catalog& getCatalog() { return catalog_; }
    const Catalog& getCatalog() const { return catalog_; }

    // Statistics
    size_t getTableCount() const;
    std::vector<std::string> getTableNames() const;
    std::string getDatabaseInfo() const;

private:
    Catalog catalog_;
    ExecutionEngine execution_engine_;
    bool initialized_ = false;
};

} // namespace velodb
