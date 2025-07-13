#include "execution/operator/nested_loop_join_operator.hpp"
#include "execution/expression.hpp"
#include <stdexcept>

namespace velodb {

// NestedLoopJoinOperator implementation
NestedLoopJoinOperator::NestedLoopJoinOperator(std::unique_ptr<AbstractOperator> left_child,
    std::unique_ptr<AbstractOperator> right_child,
    std::unique_ptr<AbstractExpression> join_predicate,
    JoinType join_type)
    : AbstractOperator(nullptr)
    , // TODO: Create joined schema
    left_child_(std::move(left_child))
    , right_child_(std::move(right_child))
    , join_predicate_(std::move(join_predicate))
    , join_type_(join_type)
{
    // TODO: Create output schema by combining left and right schemas
}

void NestedLoopJoinOperator::init()
{
    left_child_->init();
    right_child_->init();
    current_left_index_ = 0;
    current_right_index_ = 0;
    initialized_ = false;
}

bool NestedLoopJoinOperator::nextRowId(RowId* /*row_id*/)
{
    // TODO: Implement nested loop join with late materialization
    throw std::runtime_error("NestedLoopJoinOperator::nextRowId not implemented");
}

void NestedLoopJoinOperator::materializeRowIds(const std::vector<RowId>& /*row_ids*/,
    const std::vector<size_t>& /*column_indices*/,
    std::vector<Tuple>* /*tuples*/)
{
    // TODO: Implement nested loop join materialization
    throw std::runtime_error("NestedLoopJoinOperator::materializeRowIds not implemented");
}

void NestedLoopJoinOperator::reset()
{
    left_child_->reset();
    right_child_->reset();
    current_left_index_ = 0;
    current_right_index_ = 0;
    initialized_ = false;
}

} // namespace velodb
