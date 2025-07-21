#include "execution/operator/sort_operator.hpp"
#include "execution/expression.hpp"
#include <stdexcept>

namespace velodb {

// SortOperator implementation
SortOperator::SortOperator(std::unique_ptr<AbstractOperator> child,
    std::vector<std::unique_ptr<AbstractExpression>> sort_expressions,
    std::vector<bool> ascending_flags)
    : AbstractOperator(child->getOutputSchema().clone())
    , child_(std::move(child))
    , sort_expressions_(std::move(sort_expressions))
    , ascending_flags_(std::move(ascending_flags))
{
}

void SortOperator::init()
{
    // TODO: Implement sort operator initialization
    child_->init();
    sorted_ = false;
    current_index_ = 0;
    // TODO: Create sorter with sort expressions and flags
}

void SortOperator::reset()
{
    child_->reset();
    sorted_ = false;
    current_index_ = 0;
    sorted_row_ids_.clear();
}

bool SortOperator::nextRowId(RowId* /*row_id*/)
{
    // TODO: Implement sort with late materialization
    throw std::runtime_error("SortOperator::nextRowId not implemented");
}

} // namespace velodb
