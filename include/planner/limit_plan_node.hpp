#pragma once

#include "planner/abstract_plan_node.hpp"
#include <memory>
#include <string>

namespace velodb {

// Limit plan node
class LimitPlanNode : public AbstractPlanNode {
public:
    LimitPlanNode(std::unique_ptr<Schema> output_schema, size_t limit, size_t offset = 0);
    ~LimitPlanNode() override = default;

    std::unique_ptr<AbstractOperator> createOperator(ExecutionContext& context) const override;
    std::string toString() const override;

    size_t getLimit() const { return limit_; }
    size_t getOffset() const { return offset_; }

private:
    size_t limit_;
    size_t offset_;
};

} // namespace velodb
