#pragma once

#include "catalog/column.hpp"
#include "types/data_type.hpp"
#include "common/non_copyable.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace velodb {

class Schema : private NonCopyable {
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

    // Create a deep copy of this schema
    std::unique_ptr<Schema> clone() const;

    std::string toString() const;

    // Iterator support
    auto begin() const { return columns_.begin(); }
    auto end() const { return columns_.end(); }

private:

    std::vector<ColumnInfo> columns_;
    std::unordered_map<std::string, size_t> column_name_to_index_;
};

} // namespace velodb
