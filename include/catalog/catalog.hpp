#pragma once

#include "schema.hpp"
#include "table.hpp"
#include <memory>
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

    TableBase* getTable(const std::string& table_name) const;
    Table* getMutableTable(const std::string& table_name) const;

    // View management
    bool createView(const std::string& view_name,
        std::unique_ptr<Schema> schema,
        std::vector<Tuple> materialized_tuples);
    bool dropView(const std::string& view_name);
    bool hasView(const std::string& view_name) const;

    View* getView(const std::string& view_name) const;

    // Schema management
    const Schema* getTableSchema(const std::string& table_name) const;

    // Catalog information
    std::vector<std::string> getTableNames() const;
    std::vector<std::string> getViewNames() const;
    size_t getTableCount() const { return tables_.size(); }
    size_t getViewCount() const { return views_.size(); }

    // Statistics (for query optimization)
    size_t getTableRowCount(const std::string& table_name) const;

    // Utility methods
    void clear();
    std::string toString() const;

private:
    std::unordered_map<std::string, std::unique_ptr<Table>> tables_;
    std::unordered_map<std::string, std::unique_ptr<View>> views_;
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
    std::vector<Column> current_columns_;
};

} // namespace velodb
