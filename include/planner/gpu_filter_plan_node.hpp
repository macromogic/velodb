#pragma once

#include "expression/expression.hpp"
#include "planner/abstract_plan_node.hpp"

#include <memory>
#include <string>

namespace velodb {

/**
 * @brief Plan node for GPU-based filtering.
 *
 * This is used to apply filter predicates on data that is already on the GPU,
 * such as intermediate join results. Unlike FilterCompactionPlanNode which relies
 * on a pre-computed $_mask column, this node evaluates the predicate directly.
 */
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
