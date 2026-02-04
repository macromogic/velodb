#pragma once

#include "expression/expression.hpp"
#include "planner/abstract_plan_node.hpp"

#include <memory>
#include <string>

namespace velodb {

class GpuFilterPlanNode : public AbstractPlanNode {
public:
    GpuFilterPlanNode(Schema output_schema, std::unique_ptr<AbstractExpression> predicate);
    ~GpuFilterPlanNode() override = default;

    std::unique_ptr<AbstractOperator> createOperator(ExecutionContext& context) const override;
    std::string toString() const override;

    const AbstractExpression& getPredicate() const { return *predicate_; }

private:
    std::unique_ptr<AbstractExpression> predicate_;
};

} // namespace velodb
