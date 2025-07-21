#include "execution/operator/scan_filter_operator.hpp"
#include "execution/execution_engine.hpp"
#include "execution/expression.hpp"
#include <stdexcept>
#include <sstream>

namespace velodb {

ScanFilterOperator::ScanFilterOperator(const TableBase& table, const std::unique_ptr<AbstractExpression>& predicate)
    : AbstractOperator(table.getSchema().clone())
    , table_(table)
    , predicate_(predicate)
    , initialized_(false)
{
}

std::unique_ptr<QueryResult> ScanFilterOperator::execute()
{
    // ScanFilterOperator is a leaf operator - no children to execute
    // Just return row IDs that match the filter predicate
    
    auto result = std::make_unique<QueryResult>(output_schema_->clone());
    
    init();
    RowId row_id;
    while (nextRowId(&row_id)) {
        // For late materialization, ScanFilter only collects row IDs
        // Actual materialization happens at the ProjectionOperator
        // So we create an empty tuple with just the row ID for now
        Tuple tuple(*output_schema_);
        // TODO: In late materialization, we shouldn't materialize here
        // This is just for compatibility with current QueryResult
        result->addTuple(std::move(tuple));
    }
    
    return result;
}

void ScanFilterOperator::init()
{
    iterator_ = table_.getIterator();
    initialized_ = true;
}

void ScanFilterOperator::reset()
{
    if (iterator_) {
        iterator_->reset();
    }
}

bool ScanFilterOperator::nextRowId(RowId* row_id)
{
    if (!initialized_) {
        throw std::runtime_error("Operator not initialized");
    }

    while (iterator_->hasNext()) {
        const Tuple& current_tuple = iterator_->next();
        *row_id = iterator_->getCurrentRowId();

        // Apply predicate if present
        if (predicate_) {
            Value const result = predicate_->evaluate(&current_tuple, &table_.getSchema());
            if (result.isNull() || !result.getBoolean()) {
                continue; // Skip this tuple
            }
        }

        return true;
    }

    return false;
}

std::string ScanFilterOperator::toString() const
{
    std::stringstream ss;
    ss << "ScanFilterOperator(" << table_.getName() << ")";
    if (predicate_) {
        ss << " WHERE " << predicate_->toString();
    }
    return ss.str();
}

} // namespace velodb
