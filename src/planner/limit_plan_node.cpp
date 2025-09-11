#include "planner/limit_plan_node.hpp"

#include "common/exception.hpp"

#include <stdexcept>

namespace velodb {

// LimitPlanNode implementation
LimitPlanNode::LimitPlanNode(Schema output_schema, size_t limit, size_t offset)
    : AbstractPlanNode(PlanType::LIMIT, std::move(output_schema))
    , limit_(limit)
    , offset_(offset)
{
}

std::unique_ptr<AbstractOperator> LimitPlanNode::createOperator([[maybe_unused]] ExecutionContext& context) const
{
    VELODB_ASSERT_MSG(children_.size() == 1, "LimitPlanNode must have exactly one child");

    auto child_operator = children_[0]->createOperator(context);
    return std::make_unique<LimitOperator>(context, output_schema_.clone(), std::move(child_operator), limit_, offset_);
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
