#include "planner/scan_filter_plan_node.hpp"
#include "execution/operator/scan_filter_operator.hpp"
#include <sstream>

namespace velodb {

// ScanFilterPlanNode implementation
ScanFilterPlanNode::ScanFilterPlanNode(const TableBase& table, std::unique_ptr<AbstractExpression> predicate)
    : AbstractPlanNode(PlanType::SCAN_FILTER, Schema::scanFilterSchema())
    , table_(table)
    , predicate_(std::move(predicate))
{
}

std::unique_ptr<AbstractOperator> ScanFilterPlanNode::createOperator([[maybe_unused]] ExecutionContext* context) const
{
    return std::make_unique<ScanFilterOperator>(table_, predicate_);
}

std::string ScanFilterPlanNode::toString() const
{
    std::stringstream ss;
    ss << "ScanFilter(" << table_.getName() << ")";
    if (predicate_) {
        ss << " WHERE " << predicate_->toString();
    }
    return ss.str();
}

} // namespace velodb
