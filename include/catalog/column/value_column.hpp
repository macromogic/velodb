#pragma once

#include "catalog/column/column_base.hpp"
#include "data/value.hpp"
#include "data/value_vector.hpp"

#include <functional>

namespace velodb {

// Forward declarations
class ColumnInfo;
class ViewColumn;

class ValueColumn : public ColumnBase {
public:
    ValueColumn(std::string name,
                std::unique_ptr<DataType> type,
                bool is_nullable = true,
                bool is_unique = false,
                bool is_primary_key = false);

    explicit ValueColumn(const ColumnInfo& info);

    void resize(size_t new_size);
    void reserve(size_t new_capacity);
    size_t size() const override;
    Value get(size_t row) const override;
    Value operator[](size_t row);
    void append(const Value& value);
    void fill(const Value& value, size_t count);

    DataType& getType() const override { return *type_; }
    const ValueVector& getValues() const { return values_; } // For view creation

    ViewColumn view() const override;
    ViewColumn viewAs(std::string alias) const override;

    // Enhanced view creation methods (similar to Table interface)
    ViewColumn slice(size_t start_row, size_t end_row) const;
    ViewColumn indices(const std::vector<size_t>& indices) const;
    ViewColumn filterValues(std::function<bool(const Value&)> predicate) const;

    std::string toString() const override;

private:
    std::unique_ptr<DataType> type_;
    ValueVector values_; // Stores column values
};

} // namespace velodb
