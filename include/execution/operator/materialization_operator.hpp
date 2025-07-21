#pragma once

#include "execution/operator/abstract_operator.hpp"
#include "catalog/table.hpp"
#include <memory>
#include <vector>

namespace velodb {

// Forward declarations
class QueryResult;

// Materialization operator - responsible for final tuple materialization
// This operator sits at the top of the query plan and materializes row IDs
// into actual tuples with the requested columns
class MaterializationOperator : public AbstractOperator {
public:
    MaterializationOperator(std::unique_ptr<AbstractOperator> child,
                           const TableBase& table,
                           const std::vector<size_t>& column_indices);
    ~MaterializationOperator() override = default;

    void init() override;
    void reset() override;

    // Late materialization interface - not used for final operator
    bool nextRowId(RowId* row_id) override;
    
    // Execution interface - materializes all row IDs from child into tuples
    std::unique_ptr<QueryResult> execute();

private:
    // Helper methods
    void materializeColumns(const std::vector<RowId>& row_ids, QueryResult* result);
    static std::unique_ptr<Schema> createMaterializedSchema(
        const Schema& table_schema, 
        const std::vector<size_t>& column_indices);

    std::unique_ptr<AbstractOperator> child_;
    const TableBase& table_;
    std::vector<size_t> column_indices_;  // Which columns to materialize
};

} // namespace velodb
