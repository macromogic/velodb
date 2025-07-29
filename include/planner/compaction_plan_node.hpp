#pragma once

#include "catalog/table.hpp"
#include "expression/expression.hpp"
#include "planner/abstract_plan_node.hpp"
#include <memory>
#include <string>

namespace velodb {

// Compaction plan node
class CompactionPlanNode : public AbstractPlanNode {
public:
    explicit CompactionPlanNode(std::unique_ptr<Schema> output_schema);
    ~CompactionPlanNode() override = default;

    std::unique_ptr<AbstractOperator> createOperator(ExecutionContext& context) const override;
    std::string toString() const override;
};

} // namespace velodb
