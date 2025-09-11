#pragma once

#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include "common/copy_traits.hpp"
#include "data/value.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace velodb {

class TableBuilder : private NonCopyable {
public:
    TableBuilder(std::string name, Schema schema);
    TableBuilder(TableBuilder&& other) noexcept = default;
    TableBuilder& operator=(TableBuilder&& other) noexcept = default;

    ~TableBuilder() = default;

    TableBuilder& insertRow(const std::vector<Value>& values);
    TableBuilder& insertRow(std::vector<Value>&& values);
    TableBuilder& insertRows(const std::vector<std::vector<Value>>& rows);
    TableBuilder& insertRows(std::vector<std::vector<Value>>&& rows);
    TableBuilder& reserveRows(size_t row_count);

    // Inspection methods
    const std::string& getName() const { return name_; }

    Table build() &&;

private:
    std::string name_;
    Schema schema_;
    size_t num_columns_;
    size_t num_rows_ = 0;
    std::vector<std::vector<Value>> pending_columns_;

    void validateRowSize(const std::vector<Value>& row) const;
};

} // namespace velodb
