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

    return fmt::format("VeloDB Database Information:\n"
                       "  Tables: {}\n"
                       "\nCatalog Details:\n"
                       "{}",
                       getTableCount(),
                       catalog_);
}

} // namespace velodb
