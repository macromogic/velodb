#pragma once

#include "catalog/column.hpp"
#include "common/copy_traits.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace velodb {

class ColumnInfo;

class Schema : private NonCopyable, public Cloneable<Schema> {
public:
    Schema() = default;
    explicit Schema(std::vector<ColumnInfo> columns);
    ~Schema() = default;

    // Move constructor and assignment
    Schema(Schema&& other) noexcept = default;
    Schema& operator=(Schema&& other) noexcept = default;

    void addColumnInfo(ColumnInfo column);
    const ColumnInfo& getColumnInfo(size_t index) const;
    const ColumnInfo& getColumnInfo(const std::string& name) const;
    size_t getColumnIndex(const std::string& name) const;
    size_t getColumnCount() const { return columns_.size(); }

    bool hasColumn(const std::string& name) const;

    std::string toString() const;

    // Iterator support
    auto begin() const { return columns_.begin(); }
    auto end() const { return columns_.end(); }

private:
    friend class Cloneable<Schema>;
    friend class Table;
    Schema cloneImpl() const;

    std::vector<ColumnInfo> columns_;
    std::unordered_map<std::string, size_t> column_name_to_index_;

    std::unordered_map<std::string, size_t> getColumnNameToIndexMap() && { return std::move(column_name_to_index_); }
};

} // namespace velodb
