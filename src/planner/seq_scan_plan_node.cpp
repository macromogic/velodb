#include "planner/seq_scan_plan_node.hpp"
#include "execution/operator/seq_scan_operator.hpp"
#include <sstream>

namespace velodb {

// SeqScanPlanNode implementation
SeqScanPlanNode::SeqScanPlanNode(const TableBase& table, std::unique_ptr<AbstractExpression> predicate)
    : AbstractPlanNode(PlanType::SEQ_SCAN, table.getSchema().clone())
    , table_(table)
    , predicate_(std::move(predicate))
{
}

std::unique_ptr<AbstractOperator> SeqScanPlanNode::createOperator([[maybe_unused]] ExecutionContext* context) const
{
    // TODO: Use context for transaction management, buffer pool, etc.
    // Cast away const to get non-const reference for SeqScanOperator
    auto& table_ref = const_cast<TableBase&>(table_);

    // Clone predicate if it exists
    std::unique_ptr<AbstractExpression> pred_clone = nullptr;
    if (predicate_) {
        // TODO: Implement Clone method for AbstractExpression
        // For now, pass nullptr
        pred_clone = nullptr;
    }

    return std::make_unique<SeqScanOperator>(table_ref, std::move(pred_clone));
}

std::string SeqScanPlanNode::toString() const
{
    std::stringstream ss;
    ss << "SeqScan(" << table_.getName() << ")";
    if (predicate_) {
        ss << " WHERE " << predicate_->toString();
    }
    return ss.str();
}

} // namespace velodb
