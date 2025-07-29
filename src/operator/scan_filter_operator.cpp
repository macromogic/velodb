#include "operator/scan_filter_operator.hpp"
#include "execution/execution_engine.hpp"
#include "expression/expression.hpp"
#include <stdexcept>
#include <sstream>

namespace velodb {

ScanFilterOperator::ScanFilterOperator(Catalog& catalog, const TableBase& table, const std::unique_ptr<AbstractExpression>& predicate)
    : UnaryOperator(catalog, table.getSchema().cloneUnique(), nullptr) // NOTE: May support child operators in future
    , table_(table)
    , predicate_(predicate)
{
}

Result<View> ScanFilterOperator::execute() const
{
    ValueColumn& rowids = catalog_.createTemporaryColumn("$_rowid", std::make_unique<BigIntType>());
    ValueColumn& masks = catalog_.createTemporaryColumn("$_mask", std::make_unique<BooleanType>());
    rowids.reserve(table_.getRowCount());
    masks.reserve(table_.getRowCount());

    RowId row_id = 0;
    for (const auto& tuple : table_) {
        Value result = predicate_ ? predicate_->evaluate(tuple, table_.getSchema()) : Value::createBoolean(true);
        rowids.append(Value::createBigInt(row_id));
        masks.append(result);
        ++row_id;
    }

    auto view = table_.viewAs("scan_filter_result");
    view.addColumn(rowids.view());
    view.addColumn(masks.view());
    return Result<View>::success(std::move(view));
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
