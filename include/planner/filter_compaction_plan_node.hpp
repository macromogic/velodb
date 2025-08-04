#pragma once

#include "catalog/table.hpp"
#include "expression/expression.hpp"
#include "planner/abstract_plan_node.hpp"

#include <memory>
#include <string>

namespace velodb {

// Compaction plan node
class FilterCompactionPlanNode : public AbstractPlanNode {
public:
    explicit FilterCompactionPlanNode(std::unique_ptr<Schema> output_schema);
    ~FilterCompactionPlanNode() override = default;

    std::unique_ptr<AbstractOperator> createOperator(ExecutionContext& context) const override;
    std::string toString() const override;
};

} // namespace velodb
