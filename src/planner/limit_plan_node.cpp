#include "planner/limit_plan_node.hpp"

#include "common/exception.hpp"

#include <stdexcept>

namespace velodb {

// LimitPlanNode implementation
LimitPlanNode::LimitPlanNode(std::unique_ptr<Schema> output_schema, size_t limit, size_t offset)
    : AbstractPlanNode(PlanType::LIMIT, std::move(output_schema))
    , limit_(limit)
    , offset_(offset)
{
}

std::unique_ptr<AbstractOperator> LimitPlanNode::createOperator([[maybe_unused]] ExecutionContext& context) const
{
    // TODO: Implement limit operator creation
    VELODB_THROW(ExecutionError, "LimitPlanNode::createOperator not implemented");
}

std::string LimitPlanNode::toString() const
{
    if (offset_ > 0) {
        return fmt::format("Limit({}, offset={})", limit_, offset_);
    } else {
        return fmt::format("Limit({})", limit_);
    }
}

} // namespace velodb
