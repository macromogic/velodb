#pragma once

#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include "common/copy_traits.hpp"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace velodb {

// Forward declarations
class Column;

// Database catalog for managing tables and schemas
class Catalog : private NonCopyable {
public:
    // Table management
    bool addTable(Table&& table);
    bool hasTable(const std::string& table_name) const;

    std::optional<std::reference_wrapper<const Table>> getTable(const std::string& table_name) const;
    std::optional<std::reference_wrapper<const Table>> getTable(const char* table_name) const;

    // Catalog information
    std::vector<std::string> getTableNames() const;
    size_t getTableCount() const { return tables_.size(); }

    // Statistics (for query optimization)
    size_t getTableRowCount(const std::string& table_name) const;

    // Utility methods
    void clear();
    std::string toString() const;

private:
    std::unordered_map<std::string, Table> tables_;
};

} // namespace velodb
