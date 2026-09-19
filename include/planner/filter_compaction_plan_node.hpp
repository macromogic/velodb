#pragma once

#include "catalog/table.hpp"
#include "expression/expression.hpp"
#include "planner/abstract_plan_node.hpp"

#include <memory>
#include <string>
#include <vector>

namespace velodb {

// Compaction plan node
class FilterCompactionPlanNode : public AbstractPlanNode {
public:
    explicit FilterCompactionPlanNode(Schema output_schema);
    ~FilterCompactionPlanNode() override = default;

    void setCompactColumns(std::vector<bool> compact_columns);
    const std::vector<bool>& getCompactColumns() const { return compact_columns_; }

    std::unique_ptr<AbstractOperator> createOperator(ExecutionContext& context) const override;
    std::string toString() const override;

private:
    // Empty means compact every column, preserving the original behavior.
    std::vector<bool> compact_columns_;
};

} // namespace velodb
