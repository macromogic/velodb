#include "catalog/catalog.hpp"
#include <sstream>
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

Table* Catalog::getTable(const std::string& table_name) const
{
    auto it = tables_.find(table_name);
    if (it == tables_.end()) {
        return nullptr;
    }
    return it->second.get();
}

ValueColumn& Catalog::createTemporaryColumn(const std::string& column_name, std::unique_ptr<DataType> type)
{
    temporary_columns_.emplace_back(column_name, std::move(type));
    return temporary_columns_.back();
}

const Schema* Catalog::getTableSchema(const std::string& table_name) const
{
    TableBase* table = getTable(table_name);
    if (table != nullptr) {
        return &table->getSchema();
    }
    return nullptr;
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
    TableBase* table = getTable(table_name);
    if (table != nullptr) {
        return table->getRowCount();
    }
    return 0;
}

void Catalog::clear()
{
    tables_.clear();
}

std::string Catalog::toString() const
{
    std::stringstream ss;
    ss << "Catalog: " << tables_.size() << " tables\n";

    for (const auto& pair : tables_) {
        ss << "  Table: " << pair.first << " " << pair.second->getSchema().toString() << "\n";
    }

    return ss.str();
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
