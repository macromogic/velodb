#include "execution/operator/projection_operator.hpp"
#include "execution/operator/scan_filter_operator.hpp"
#include "execution/expression.hpp"
#include "execution/execution_engine.hpp"
#include "catalog/table.hpp"

namespace velodb {

// ProjectionOperator implementation
ProjectionOperator::ProjectionOperator(std::unique_ptr<Schema> output_schema,
    std::unique_ptr<AbstractOperator> child,
    std::vector<std::unique_ptr<AbstractExpression>> expressions)
    : AbstractOperator(std::move(output_schema))
    , expressions_(std::move(expressions))
{
    // Add child to the children vector for proper tree structure
    if (child) {
        addChild(std::move(child));
    }
}

void ProjectionOperator::init()
{
    if (!children_.empty()) {
        children_[0]->init();
    }
}

void ProjectionOperator::reset()
{
    if (!children_.empty()) {
        children_[0]->reset();
    }
}

bool ProjectionOperator::nextRowId(RowId* row_id)
{
    // ProjectionOperator in this design just passes through row IDs
    // The actual projection evaluation happens during materialization
    if (!children_.empty()) {
        return children_[0]->nextRowId(row_id);
    }
    return false;
}

std::unique_ptr<QueryResult> ProjectionOperator::execute()
{
    // PROPER TREE TRAVERSAL EXECUTION:
    // 1. First execute all children (bottom-up traversal)
    // 2. Then execute this operator's logic
    
    // Step 1: Execute child operators first (tree traversal)
    if (!children_.empty()) {
        // For projection operator, we don't actually execute the child
        // because we need to collect row IDs, not materialized results
        // The child execution is implicit through the nextRowId() calls
    }
    
    // Step 2: Initialize the pipeline
    init();
    
    // Step 3: Collect all row IDs from the child pipeline (Late Materialization)
    std::vector<RowId> row_ids;
    RowId row_id;
    while (nextRowId(&row_id)) {
        row_ids.push_back(row_id);
    }
    
    // Step 4: Find the source table for materialization
    const TableBase* source_table = findSourceTable();
    if (!source_table) {
        throw std::runtime_error("Could not find source table for materialization");
    }
    
    // Step 5: Create the result
    auto result = std::make_unique<QueryResult>(output_schema_->clone());
    
    // Step 6: MATERIALIZATION + PROJECTION - for each row ID, materialize and project
    for (const RowId& rid : row_ids) {
        // Materialize the full tuple from the source table
        const auto* table = dynamic_cast<const Table*>(source_table);
        if (!table) {
            throw std::runtime_error("Source table is not a concrete Table - views not supported yet");
        }
        
        // Create a tuple by getting all column values for this row
        std::vector<Value> full_values;
        full_values.reserve(source_table->getSchema().getColumnCount());
        for (size_t col_idx = 0; col_idx < source_table->getSchema().getColumnCount(); ++col_idx) {
            full_values.push_back(table->getValue(rid, col_idx));
        }
        Tuple full_tuple(source_table->getSchema(), std::move(full_values));
        
        // Evaluate projection expressions on the materialized tuple
        std::vector<Value> projected_values;
        projected_values.reserve(expressions_.size());
        
        for (const auto& expr : expressions_) {
            Value projected_value = expr->evaluate(&full_tuple, &source_table->getSchema());
            projected_values.push_back(std::move(projected_value));
        }
        
        // Add projected row to result
        result->addRow(std::move(projected_values));
    }
    
    return result;
}

const TableBase* ProjectionOperator::findSourceTable() const
{
    // Walk down the operator tree to find the scan operator with the source table
    // This is a simple implementation - could be enhanced for joins, etc.
    
    if (children_.empty()) {
        return nullptr;
    }
    
    AbstractOperator* current = children_[0].get(); // First child
    
    while (current) {
        // Check if this is a scan filter operator
        if (auto* scan_filter = dynamic_cast<const ScanFilterOperator*>(current)) {
            return &scan_filter->getTable();
        }
        
        // For other operators, traverse their first child
        if (!current->getChildren().empty()) {
            current = current->getChildren()[0].get();
        } else {
            break;
        }
    }
    
    return nullptr;
}

} // namespace velodb
