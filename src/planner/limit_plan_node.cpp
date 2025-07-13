#include "planner/limit_plan_node.hpp"
#include <stdexcept>
#include <sstream>

namespace velodb {

// LimitPlanNode implementation
LimitPlanNode::LimitPlanNode(std::unique_ptr<Schema> output_schema, size_t limit, size_t offset)
    : AbstractPlanNode(PlanType::LIMIT, std::move(output_schema))
    , limit_(limit)
    , offset_(offset)
{
}

std::unique_ptr<AbstractOperator> LimitPlanNode::createOperator([[maybe_unused]] ExecutionContext* context) const
{
    // TODO: Implement limit operator creation
    throw std::runtime_error("LimitPlanNode::createOperator not implemented");
}

std::string LimitPlanNode::toString() const
{
    std::stringstream ss;
    ss << "Limit(" << limit_;
    if (offset_ > 0) {
        ss << ", offset=" << offset_;
    }
    ss << ")";
    return ss.str();
}

} // namespace velodb
