#include "velodb.hpp"
#include <sstream>
#include <stdexcept>

namespace velodb {

// TODO: Implement full Database class functionality

// Database implementation
Database::Database()
{
    catalog_ = std::make_unique<Catalog>();
    execution_engine_ = std::make_unique<ExecutionEngine>(*catalog_);
}

bool Database::initialize()
{
    // TODO: Implement database initialization
    initialized_ = true;
    return true;
}

void Database::shutdown()
{
    // TODO: Implement database shutdown
    initialized_ = false;
}

bool Database::createTable(const std::string& table_name, std::unique_ptr<Schema> schema)
{
    if (!initialized_)
        return false;
    return catalog_->createTable(table_name, std::move(schema));
}

bool Database::dropTable(const std::string& table_name)
{
    if (!initialized_)
        return false;
    return catalog_->dropTable(table_name);
}

bool Database::hasTable(const std::string& table_name) const
{
    if (!initialized_)
        return false;
    return catalog_->hasTable(table_name);
}

std::optional<std::reference_wrapper<Table>> Database::getTable(const std::string& table_name) const
{
    if (!initialized_)
        return std::nullopt;
    return catalog_->getTable(table_name);
}

bool Database::insertTuple(const std::string& table_name, const Tuple& tuple)
{
    if (!initialized_)
        return false;
    auto table = catalog_->getTable(table_name);
    if (!table)
        return false;

    // Convert tuple to values vector
    std::vector<Value> values;
    values.reserve(tuple.getColumnCount());
    for (size_t i = 0; i < tuple.getColumnCount(); ++i) {
        values.push_back(tuple.getValue(i));
    }
    table->get().insertRow(values);
    return true;
}

bool Database::insertTuple(const std::string& table_name, Tuple&& tuple)
{
    if (!initialized_)
        return false;
    auto table = catalog_->getTable(table_name);
    if (!table)
        return false;

    // Convert tuple to values vector
    std::vector<Value> values;
    values.reserve(tuple.getColumnCount());
    for (size_t i = 0; i < tuple.getColumnCount(); ++i) {
        values.push_back(std::move(const_cast<Tuple&>(tuple).getValue(i)));
    }
    table->get().insertRow(std::move(values));
    return true;
}

Result<View> Database::executeQuery(const std::string& sql)
{
    if (!initialized_) {
        return Result<View>::failure("Database not initialized");
    }
    return execution_engine_->executeQuery(sql);
}

size_t Database::getTableCount() const
{
    if (!initialized_)
        return 0;
    return catalog_->getTableNames().size();
}

std::vector<std::string> Database::getTableNames() const
{
    if (!initialized_)
        return {};
    return catalog_->getTableNames();
}

std::string Database::getDatabaseInfo() const
{
    if (!initialized_)
        return "Database not initialized";

    std::stringstream ss;
    ss << "VeloDB Database Information:\n";
    ss << "  Tables: " << getTableCount() << "\n";
    ss << "\nCatalog Details:\n";
    ss << catalog_->toString();

    return ss.str();
}

} // namespace velodb
