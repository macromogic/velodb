#include "execution/operator/limit_operator.hpp"

namespace velodb {

// LimitOperator implementation
LimitOperator::LimitOperator(std::unique_ptr<AbstractOperator> child, size_t limit, size_t offset)
    : AbstractOperator(child->getOutputSchema().clone())
    , child_(std::move(child))
    , limit_(limit)
    , offset_(offset)
{
}

void LimitOperator::init()
{
    child_->init();
    current_count_ = 0;
    skipped_count_ = 0;
}

void LimitOperator::reset()
{
    child_->reset();
    current_count_ = 0;
    skipped_count_ = 0;
}

bool LimitOperator::nextRowId(RowId* row_id)
{
    // TODO: Implement limit with late materialization
    // Skip offset rows
    while (skipped_count_ < offset_ && child_->nextRowId(row_id)) {
        skipped_count_++;
    }

    // Return up to limit rows
    if (current_count_ < limit_ && child_->nextRowId(row_id)) {
        current_count_++;
        return true;
    }

    return false;
}

void LimitOperator::materializeRowIds(const std::vector<RowId>& row_ids,
    const std::vector<size_t>& column_indices,
    std::vector<Tuple>* tuples)
{
    // TODO: Implement limit materialization
    child_->materializeRowIds(row_ids, column_indices, tuples);
}

} // namespace velodb
