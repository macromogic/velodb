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
{
}

Result<View> ScanFilterOperator::next() const
{
    ValueColumn& rowids = context_.createTemporaryColumn("$_rowid", std::make_unique<BigIntType>());
    ValueColumn& masks = context_.createTemporaryColumn("$_mask", std::make_unique<BooleanType>());
    rowids.reserve(table_.getRowCount());
    masks.reserve(table_.getRowCount());

    uint64_t row_id = 0;
    for (const auto& tuple : table_) {
        Value result = predicate_ ? predicate_->evaluate(tuple, table_.getSchema()) : Value::createBoolean(true);
        rowids.append(Value::createBigInt(row_id));
        masks.append(result);
        ++row_id;
    }

    auto view = table_.view();
    view.addColumn(rowids.view());
    view.addColumn(masks.view());
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
