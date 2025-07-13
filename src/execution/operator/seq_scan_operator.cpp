#include "execution/operator/seq_scan_operator.hpp"
#include "execution/expression.hpp"
#include <stdexcept>

namespace velodb {

// SeqScanOperator implementation
SeqScanOperator::SeqScanOperator(TableBase& table,
    std::unique_ptr<AbstractExpression> predicate)
    : AbstractOperator(table.getSchema().clone())
    , table_(table)
    , predicate_(std::move(predicate))
{
}

void SeqScanOperator::init()
{
    iterator_ = table_.getIterator();
    initialized_ = true;
}

// Legacy Next() method removed - use late materialization only

void SeqScanOperator::reset()
{
    if (iterator_) {
        iterator_->reset();
    }
}

bool SeqScanOperator::nextRowId(RowId* row_id)
{
    // TODO: Implement late materialization row ID iteration
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

void SeqScanOperator::materializeRowIds(const std::vector<RowId>& row_ids,
    const std::vector<size_t>& column_indices,
    std::vector<Tuple>* tuples)
{
    // TODO: Implement efficient late materialization
    tuples->clear();
    tuples->reserve(row_ids.size());

    for (RowId const row_id : row_ids) {
        if (!table_.isView()) {
            const auto& table = dynamic_cast<const Table&>(table_);
            std::vector<Value> values = table.getValues(row_id, column_indices);

            // Create a schema with only the requested columns
            std::vector<Column> columns;
            for (size_t const col_idx : column_indices) {
                const Column& orig_col = table_.getSchema().getColumn(col_idx);
                columns.emplace_back(orig_col.getName(),
                    DataType::createType(orig_col.getType().getTypeId()),
                    orig_col.isNullable());
            }
            Schema const partial_schema(std::move(columns));

            tuples->emplace_back(partial_schema, std::move(values));
        }
    }
}

} // namespace velodb
