#pragma once

#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include "common/copy_traits.hpp"
#include "data/oblivious_table_manager.hpp"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace velodb {

// Forward declarations
class Column;
class TaskManager;

// Database catalog for managing tables and schemas
class Catalog : private NonCopyable {
public:
    // Table management
    bool addTable(Table&& table);
    bool hasTable(const std::string& table_name) const;

    std::optional<std::reference_wrapper<const Table>> getTable(const std::string& table_name) const;
    std::optional<std::reference_wrapper<const Table>> getTable(const char* table_name) const;

    // Non-const version for oblivious table registration
    Table* getTableMutable(const std::string& table_name);

    // Catalog information
    std::vector<std::string> getTableNames() const;
    size_t getTableCount() const { return tables_.size(); }

    // Statistics (for query optimization)
    size_t getTableRowCount(const std::string& table_name) const;

    // Oblivious table management
    void initObliviousManager(TaskManager& task_manager);
    bool hasObliviousManager() const { return oblivious_manager_ != nullptr; }
    ObliviousTableManager& getObliviousManager();
    const ObliviousTableManager& getObliviousManager() const;

    // Register all tables for oblivious protection
    void registerAllTablesAsOblivious();

    // Utility methods
    void clear();
    std::string toString() const;

private:
    std::unordered_map<std::string, Table> tables_;
    std::unique_ptr<ObliviousTableManager> oblivious_manager_;
};

} // namespace velodb
