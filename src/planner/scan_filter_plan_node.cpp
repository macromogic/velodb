#include "planner/execution_context.hpp"
#include "planner/scan_filter_plan_node.hpp"
#include "operator/compaction_operator.hpp"
#include "operator/scan_filter_operator.hpp"
#include <sstream>

namespace velodb {

// ScanFilterPlanNode implementation
ScanFilterPlanNode::ScanFilterPlanNode(const TableBase& table, std::unique_ptr<Schema> output_schema, std::unique_ptr<AbstractExpression> predicate)
    : AbstractPlanNode(PlanType::SCAN_FILTER, std::move(output_schema))
    , table_(table)
    , predicate_(std::move(predicate))
{
}

std::unique_ptr<AbstractOperator> ScanFilterPlanNode::createOperator(ExecutionContext& context) const
{
    auto scan_filter_op = std::make_unique<ScanFilterOperator>(context.getCatalog(), table_, predicate_);
    return std::make_unique<CompactionOperator>(context.getCatalog(), scan_filter_op->getOutputSchema().cloneUnique(), std::move(scan_filter_op));
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
