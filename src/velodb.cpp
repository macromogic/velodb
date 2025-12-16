#include "velodb.hpp"

#include "catalog/table_builder.hpp"
#include "common/fmt.hpp"
#include "cuda/warmup.hpp"

#include <fmt/core.h>

#include <stdexcept>

namespace velodb {

// Database implementation
Database::Database()
    : catalog_()
    , execution_engine_(catalog_)
{
}

Database::Database(Database&& other) noexcept
    : catalog_(std::move(other.catalog_))
    , execution_engine_(catalog_) // Re-bind to new catalog reference
    , initialized_(other.initialized_)
{
    other.initialized_ = false; // Leave other in valid but uninitialized state
}

Database& Database::operator=(Database&& other) noexcept
{
    if (this != &other) {
        // Clean up current state if needed
        if (initialized_) {
            shutdown();
        }

        // Move resources
        catalog_ = std::move(other.catalog_);
        execution_engine_ = ExecutionEngine { catalog_ }; // Reconstruct with new catalog
        initialized_ = other.initialized_;

        // Leave other in valid state
        other.initialized_ = false;
    }
    return *this;
}

void Database::initialize()
{
    auto result = runtime_warmup();
    if (!result) {
        throw std::runtime_error(fmt::format("Failed to initialize database: {}", result.error()));
    }
    initialized_ = true;
}

void Database::shutdown()
{
    initialized_ = false;
}

bool Database::createTable(const std::string& table_name, Schema schema)
{
    if (!initialized_)
        return false;
    auto builder = TableBuilder(table_name, std::move(schema));
    return catalog_.addTable(std::move(builder).build());
}

bool Database::hasTable(const std::string& table_name) const
{
    if (!initialized_)
        return false;
    return catalog_.hasTable(table_name);
}

std::optional<std::reference_wrapper<const Table>> Database::getTable(const std::string& table_name) const
{
    if (!initialized_)
        return std::nullopt;
    return catalog_.getTable(table_name);
}

Result<QueryResult> Database::executeQuery(const std::string& sql)
{
    if (!initialized_) {
        return Result<QueryResult>::failure("Database not initialized");
    }
    return execution_engine_.executeQuery(sql);
}

size_t Database::getTableCount() const
{
    if (!initialized_)
        return 0;
    return catalog_.getTableNames().size();
}

std::vector<std::string> Database::getTableNames() const
{
    if (!initialized_)
        return {};
    return catalog_.getTableNames();
}

std::string Database::getDatabaseInfo() const
{
    if (!initialized_)
        return "Database not initialized";

    return fmt::format("VelODB Database Information:\n"
                       "  Tables: {}\n"
                       "\nCatalog Details:\n"
                       "{}",
                       getTableCount(),
                       catalog_);
}

} // namespace velodb
