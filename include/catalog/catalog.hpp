#pragma once

#include "schema.hpp"
#include "table.hpp"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace velodb {

// Database catalog for managing tables and schemas
class Catalog {
public:
    Catalog() = default;
    ~Catalog() = default;

    // Delete copy constructor and assignment
    Catalog(const Catalog&) = delete;
    Catalog& operator=(const Catalog&) = delete;

    // Table management
    bool createTable(const std::string& table_name, std::unique_ptr<Schema> schema);
    bool dropTable(const std::string& table_name);
    bool hasTable(const std::string& table_name) const;

    std::optional<std::reference_wrapper<Table>> getTable(const std::string& table_name) const;
    std::optional<std::reference_wrapper<Table>> getTable(const char* table_name) const;

    // Catalog information
    std::vector<std::string> getTableNames() const;
    size_t getTableCount() const { return tables_.size(); }

    // Statistics (for query optimization)
    size_t getTableRowCount(const std::string& table_name) const;

    // Utility methods
    void clear();
    std::string toString() const;

private:
    std::unordered_map<std::string, std::unique_ptr<Table>> tables_;
    std::vector<std::unique_ptr<ValueColumn>> temporary_columns_;
};

// Catalog builder for easy setup
class CatalogBuilder {
public:
    CatalogBuilder()
        : catalog_(std::make_unique<Catalog>())
    {
    }
    ~CatalogBuilder() = default;

    // Table creation helpers
    CatalogBuilder& addTable(const std::string& table_name, std::unique_ptr<Schema> schema);
    CatalogBuilder& addIntegerColumn(const std::string& column_name, bool nullable = true);
    CatalogBuilder& addStringColumn(const std::string& column_name, size_t max_length, bool nullable = true);
    CatalogBuilder& addDoubleColumn(const std::string& column_name, bool nullable = true);
    CatalogBuilder& addBooleanColumn(const std::string& column_name, bool nullable = true);

    // Finalize current table being built
    CatalogBuilder& finishTable(const std::string& table_name);

    // Build and return the catalog
    std::unique_ptr<Catalog> build();

private:
    std::unique_ptr<Catalog> catalog_;
    std::vector<ColumnInfo> current_columns_;
};

} // namespace velodb
