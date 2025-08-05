#include "planner/seq_scan_plan_node.hpp"

#include "catalog/execution_context.hpp"
#include "common/fmt.hpp"
#include "operator/filter_compaction_operator.hpp"
#include "operator/seq_scan_operator.hpp"

namespace velodb {

// ScanFilterPlanNode implementation
ScanFilterPlanNode::ScanFilterPlanNode(const TableBase& table,
                                       std::unique_ptr<Schema> output_schema,
                                       std::unique_ptr<AbstractExpression> predicate)
    : AbstractPlanNode(PlanType::SEQ_SCAN, std::move(output_schema))
    , table_(table)
    , predicate_(std::move(predicate))
{
}

std::unique_ptr<AbstractOperator> ScanFilterPlanNode::createOperator(ExecutionContext& context) const
{
    return std::make_unique<ScanFilterOperator>(context, table_, predicate_);
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
