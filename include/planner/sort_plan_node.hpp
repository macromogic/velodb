#pragma once

#include "expression/expression.hpp"
#include "planner/abstract_plan_node.hpp"

#include <memory>
#include <string>
#include <vector>

namespace velodb {

// Sort plan node
class SortPlanNode : public AbstractPlanNode {
public:
    SortPlanNode(Schema output_schema, std::vector<size_t> order_indices, std::vector<bool> ascending_flags);
    ~SortPlanNode() override = default;

    std::unique_ptr<AbstractOperator> createOperator(ExecutionContext& context) const override;
    std::string toString() const override;

private:
    std::vector<size_t> order_indices_;
    std::vector<bool> ascending_flags_;
};

} // namespace velodb
