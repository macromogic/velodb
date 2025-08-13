#include "operator/seq_scan_operator.hpp"

#include "common/fmt.hpp"
#include "execution/execution_engine.hpp"
#include "expression/expression.hpp"

#include <stdexcept>

namespace velodb {

ScanFilterOperator::ScanFilterOperator(ExecutionContext& context,
                                       const TableBase& table,
                                       const std::unique_ptr<AbstractExpression>& predicate)
    : UnaryOperator(context,
                    table.getSchema().cloneUnique(),
                    nullptr) // NOTE: May support child operators in future
    , table_(table)
    , predicate_(predicate)
    , rowids_(context.createTemporaryColumn("$_rowid", std::make_unique<BigIntType>()))
    , masks_(context.createTemporaryColumn("$_mask", std::make_unique<BooleanType>()))
    , current_row_id_(0)
    , iterator_(table)
{
}

Result<View> ScanFilterOperator::next()
{
    size_t start_row_id = current_row_id_;
    size_t end_row_id = std::min(start_row_id + MAX_BATCH_SIZE, table_.getRowCount());
    while (current_row_id_ < end_row_id) {
        const auto& tuple = *iterator_;
        Value result = predicate_ ? predicate_->evaluate(tuple, table_.getSchema()) : Value::createBoolean(true);
        rowids_.append(Value::createBigInt(current_row_id_));
        masks_.append(result);
        ++current_row_id_;
        ++iterator_;
    }

    auto view = table_.slice(start_row_id, end_row_id);
    view.addColumn(rowids_.slice(start_row_id, end_row_id));
    view.addColumn(masks_.slice(start_row_id, end_row_id));
    return Result<View>::success(std::move(view));
}

std::string ScanFilterOperator::toString() const
{
    std::string result = fmt::format("ScanFilterOperator({})", table_.getName());
    if (predicate_) {
        result += fmt::format(" WHERE {}", *predicate_);
    }
    return result;
}

} // namespace velodb
