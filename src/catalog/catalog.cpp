#include "catalog/catalog.hpp"

#include <fmt/core.h>

#include <stdexcept>

namespace velodb {

// TODO: Implement full catalog functionality
bool Catalog::createTable(const std::string& table_name, std::unique_ptr<Schema> schema)
{
    if (hasTable(table_name)) {
        return false;
    }

    auto table_info = std::make_unique<TableInfo>(table_name, std::move(schema));
    auto table = std::make_unique<Table>(std::move(table_info));
    tables_[table_name] = std::move(table);
    return true;
}

bool Catalog::dropTable(const std::string& table_name)
{
    auto it = tables_.find(table_name);
    if (it == tables_.end()) {
        return false;
    }
    tables_.erase(it);
    return true;
}

bool Catalog::hasTable(const std::string& table_name) const
{
    return tables_.find(table_name) != tables_.end();
}

std::optional<std::reference_wrapper<Table>> Catalog::getTable(const std::string& table_name) const
{
    auto it = tables_.find(table_name);
    if (it == tables_.end()) {
        return std::nullopt;
    }
    return *it->second;
}

std::optional<std::reference_wrapper<Table>> Catalog::getTable(const char* table_name) const
{
    if (table_name == nullptr) {
        return std::nullopt;
    }
    return getTable(std::string(table_name));
}

ValueColumn& Catalog::createTemporaryColumn(const std::string& column_name, std::unique_ptr<DataType> type)
{
    auto column = std::make_unique<ValueColumn>(column_name, std::move(type));
    auto& ref = *column;
    temporary_columns_.push_back(std::move(column));
    return ref;
}

std::vector<std::string> Catalog::getTableNames() const
{
    std::vector<std::string> names;
    names.reserve(tables_.size());
    for (const auto& pair : tables_) {
        names.push_back(pair.first);
    }
    return names;
}

size_t Catalog::getTableRowCount(const std::string& table_name) const
{
    auto table = getTable(table_name);
    if (table) {
        return table->get().getRowCount();
    }
    return 0;
}

void Catalog::clear()
{
    tables_.clear();
}

std::string Catalog::toString() const
{
    std::string result = fmt::format("Catalog: {} tables\n", tables_.size());

    for (const auto& pair : tables_) {
        result += fmt::format("  Table: {} {}\n", pair.first, pair.second->getSchema().toString());
    }

    return result;
}

// TODO: Implement catalog builder functionality
CatalogBuilder& CatalogBuilder::addTable(const std::string& table_name, std::unique_ptr<Schema> schema)
{
    catalog_->createTable(table_name, std::move(schema));
    return *this;
}

CatalogBuilder& CatalogBuilder::addIntegerColumn(const std::string& column_name, bool nullable)
{
    auto type = std::make_unique<IntegerType>();
    current_columns_.emplace_back(column_name, std::move(type), nullable);
    return *this;
}

CatalogBuilder& CatalogBuilder::addStringColumn(const std::string& column_name, size_t max_length, bool nullable)
{
    auto type = std::make_unique<VarcharType>(max_length);
    current_columns_.emplace_back(column_name, std::move(type), nullable);
    return *this;
}

CatalogBuilder& CatalogBuilder::addDoubleColumn(const std::string& column_name, bool nullable)
{
    auto type = std::make_unique<DoubleType>();
    current_columns_.emplace_back(column_name, std::move(type), nullable);
    return *this;
}

CatalogBuilder& CatalogBuilder::addBooleanColumn(const std::string& column_name, bool nullable)
{
    auto type = std::make_unique<BooleanType>();
    current_columns_.emplace_back(column_name, std::move(type), nullable);
    return *this;
}

CatalogBuilder& CatalogBuilder::finishTable(const std::string& table_name)
{
    auto schema = std::make_unique<Schema>(std::move(current_columns_));
    catalog_->createTable(table_name, std::move(schema));
    current_columns_.clear();
    return *this;
}

std::unique_ptr<Catalog> CatalogBuilder::build()
{
    return std::move(catalog_);
}

} // namespace velodb
