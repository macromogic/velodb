#include "operator/projection_operator.hpp"
#include "operator/scan_filter_operator.hpp"
#include "expression/expression.hpp"
#include "execution/execution_engine.hpp"
#include "catalog/table.hpp"

namespace velodb {

// ProjectionOperator implementation
ProjectionOperator::ProjectionOperator(Catalog& catalog,
    std::unique_ptr<Schema> output_schema,
    std::unique_ptr<AbstractOperator> child,
    std::vector<std::unique_ptr<AbstractExpression>> expressions)
    : AbstractOperator(catalog, std::move(output_schema))
    , expressions_(std::move(expressions))
{
    // Add child to the children vector for proper tree structure
    if (child) {
        addChild(std::move(child));
    }
}

void ProjectionOperator::init()
{
    // if (!children_.empty()) {
    //     children_[0]->init();
    // }
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

View ProjectionOperator::execute()
{
    // PROPER TREE TRAVERSAL EXECUTION:
    // 1. First execute all children (bottom-up traversal)
    // 2. Then execute this operator's logic
    
    init();
    if (children_.empty()) {
        // TODO: Handle case with no children; use Result or empty View
        // return std::make_unique<View>(output_schema_->clone());
    }
    auto child_result = children_[0]->execute();
    
    // TODO: Do the actual projection logic here
    
    return child_result;
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
