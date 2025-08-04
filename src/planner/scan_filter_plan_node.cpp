#include "planner/scan_filter_plan_node.hpp"

#include "common/fmt.hpp"
#include "operator/compaction_operator.hpp"
#include "operator/scan_filter_operator.hpp"
#include "planner/execution_context.hpp"

namespace velodb {

// ScanFilterPlanNode implementation
ScanFilterPlanNode::ScanFilterPlanNode(const TableBase& table,
                                       std::unique_ptr<Schema> output_schema,
                                       std::unique_ptr<AbstractExpression> predicate)
    : AbstractPlanNode(PlanType::SCAN_FILTER, std::move(output_schema))
    , table_(table)
    , predicate_(std::move(predicate))
{
}

std::unique_ptr<AbstractOperator> ScanFilterPlanNode::createOperator(ExecutionContext& context) const
{
    return std::make_unique<ScanFilterOperator>(context.getCatalog(), table_, predicate_);
}

std::string ScanFilterPlanNode::toString() const
{
    std::string result = fmt::format("ScanFilter({})", table_.getName());
    if (predicate_) {
        result += fmt::format(" WHERE {}", *predicate_);
    }
    return result;
}

} // namespace velodb
