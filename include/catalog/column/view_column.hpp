#pragma once

#include "catalog/column/column_base.hpp"
#include "data/value.hpp"

#include <variant>

namespace velodb {

class ValueVector; // Forward declaration

class ViewColumn : public ColumnBase {
public:
    struct EntireView {
        // No additional data needed - views entire column
    };

    struct SliceView {
        size_t start_row;
        size_t end_row;

        SliceView(size_t start, size_t end)
            : start_row(start)
            , end_row(end)
        {
        }
    };

    struct IndicesView {
        std::vector<size_t> indices;

        explicit IndicesView(std::vector<size_t> idx)
            : indices(std::move(idx))
        {
        }
    };

    using ViewMode = std::variant<EntireView, SliceView, IndicesView>;

    ViewColumn(DataType& type, std::string name, const ValueVector& values);
    ViewColumn(DataType& type, std::string name, const ValueVector& values, size_t start_row, size_t end_row);
    ViewColumn(DataType& type, std::string name, const ValueVector& values, std::vector<size_t> indices);

    size_t size() const override;
    Value get(size_t row) const override;

    DataType& getType() const override { return type_; }

    ViewColumn view() const override;
    ViewColumn viewAs(std::string alias) const override;

    // Create new views from this view
    ViewColumn slice(size_t start_row, size_t end_row) const;
    ViewColumn indices(const std::vector<size_t>& indices) const;

    std::string toString() const override;

private:
    DataType& type_;
    const ValueVector& values_; // Reference to the original column values
    ViewMode view_mode_;

    size_t mapToActualRow(size_t view_row) const;
};

} // namespace velodb
