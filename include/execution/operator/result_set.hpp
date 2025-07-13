#pragma once

#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include <memory>
#include <vector>

namespace velodb {

// Result set for late materialization - contains row IDs and selected columns
class ResultSet {
public:
    explicit ResultSet(std::unique_ptr<Schema> schema);
    ~ResultSet() = default;

    // Move constructor and assignment
    ResultSet(ResultSet&& other) noexcept = default;
    ResultSet& operator=(ResultSet&& other) noexcept = default;

    // Delete copy constructor and assignment
    ResultSet(const ResultSet&) = delete;
    ResultSet& operator=(const ResultSet&) = delete;

    void addTuple(const Tuple& tuple);
    void addTuple(Tuple&& tuple);
    void addRowId(RowId row_id, const TableBase& table);

    [[nodiscard]] const Schema& getSchema() const { return *schema_; }
    [[nodiscard]] size_t getRowCount() const { return tuples_.size() + row_ids_.size(); }
    [[nodiscard]] bool isEmpty() const { return tuples_.empty() && row_ids_.empty(); }

    // Iterator support for materialized tuples
    [[nodiscard]] const std::vector<Tuple>& getTuples() const { return tuples_; }

    // For late materialization
    [[nodiscard]] const std::vector<RowId>& getRowIds() const { return row_ids_; }
    [[nodiscard]] const TableBase* getSourceTable() const { return source_table_; }

    void clear();

private:
    std::unique_ptr<Schema> schema_;
    std::vector<Tuple> tuples_; // Materialized tuples
    std::vector<RowId> row_ids_; // Row IDs for late materialization
    const TableBase* source_table_ { nullptr }; // Source table for row IDs
};

} // namespace velodb
