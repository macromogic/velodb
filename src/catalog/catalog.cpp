#include "catalog/catalog.hpp"

#include "catalog/column.hpp"
#include "common/fmt.hpp"

#include <fmt/core.h>

#include <stdexcept>

namespace velodb {

bool Catalog::addTable(Table&& table)
{
    const std::string& table_name = table.getName();
    if (hasTable(table_name)) {
        return false; // Table already exists
    }
    tables_.emplace(table_name, std::move(table));
    return true;
}

bool Catalog::hasTable(const std::string& table_name) const
{
    return tables_.find(table_name) != tables_.end();
}

std::optional<std::reference_wrapper<const Table>> Catalog::getTable(const std::string& table_name) const
{
    auto it = tables_.find(table_name);
    if (it == tables_.end()) {
        return std::nullopt;
    }
    return std::ref(it->second);
}

std::optional<std::reference_wrapper<const Table>> Catalog::getTable(const char* table_name) const
{
    if (table_name == nullptr) {
        return std::nullopt;
    }
    return getTable(std::string(table_name));
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

    for (const auto& [_, table] : tables_) {
        result += fmt::format("  Table: {}\n", table);
    }

    return result;
}

} // namespace velodb
