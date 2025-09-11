#include "planner/seq_scan_plan_node.hpp"

#include "catalog/execution_context.hpp"
#include "common/fmt.hpp"
#include "operator/filter_compaction_operator.hpp"
#include "operator/seq_scan_operator.hpp"

namespace velodb {

// SeqScanPlanNode implementation
SeqScanPlanNode::SeqScanPlanNode(const Table& table,
                                 Schema output_schema,
                                 std::unique_ptr<AbstractExpression> predicate)
    : AbstractPlanNode(PlanType::SEQ_SCAN, std::move(output_schema))
    , table_(table)
    , predicate_(std::move(predicate))
{
}

std::unique_ptr<AbstractOperator> SeqScanPlanNode::createOperator(ExecutionContext& context) const
{
    return std::make_unique<SeqScanOperator>(context, table_, output_schema_.clone(), predicate_);
}

std::string SeqScanPlanNode::toString() const
{
    std::string result = fmt::format("SeqScan({})", table_.getName());
    if (predicate_) {
        result += fmt::format(" WHERE {}", *predicate_);
    }
    return result;
}

} // namespace velodb
