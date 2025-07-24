#pragma once

#include "operator/abstract_operator.hpp"
#include <memory>
#include <vector>

namespace velodb {

// Forward declarations
class AbstractExpression;
class View;
class TableBase;

// Projection operator - handles final materialization and projection
class ProjectionOperator : public AbstractOperator {
public:
    ProjectionOperator(Catalog& catalog,
        std::unique_ptr<Schema> output_schema,
        std::unique_ptr<AbstractOperator> child,
        std::vector<std::unique_ptr<AbstractExpression>> expressions);
    ~ProjectionOperator() override = default;

    // Main execution interface - implements tree traversal
    View execute() override;

    void init() override;
    void reset() override;

    // Late materialization interface
    bool nextRowId(RowId* row_id) override;

private:
    std::vector<std::unique_ptr<AbstractExpression>> expressions_;
    
    // Helper method to find the base table for materialization
    const TableBase* findSourceTable() const;
};

} // namespace velodb
