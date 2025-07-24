#include "operator/scan_filter_operator.hpp"
#include "execution/execution_engine.hpp"
#include "expression/expression.hpp"
#include <stdexcept>
#include <sstream>

namespace velodb {

ScanFilterOperator::ScanFilterOperator(Catalog& catalog, const TableBase& table, const std::unique_ptr<AbstractExpression>& predicate)
    : AbstractOperator(catalog, table.getSchema().clone())
    , table_(table)
    , predicate_(predicate)
{
}

View ScanFilterOperator::execute()
{
    // ScanFilterOperator is a leaf operator - no children to execute
    // Just return row IDs that match the filter predicate
    
    ValueColumn& rowids = catalog_.createTemporaryColumn("rowids", std::make_unique<BigIntType>());
    ValueColumn& masks = catalog_.createTemporaryColumn("masks", std::make_unique<BooleanType>());
    rowids.reserve(table_.getRowCount());
    masks.reserve(table_.getRowCount());
    
    RowId row_id = 0;
    for (const auto& tuple : table_) {
        Value result = predicate_ ? predicate_->evaluate(tuple, table_.getSchema()) : Value::createBoolean(true);
        rowids.append(Value::createBigInt(row_id));
        masks.append(result);
        ++row_id;
    }

    auto view = table_.view();
    view.addColumn(rowids.view());
    view.addColumn(masks.view());
    return view;
}

void ScanFilterOperator::init()
{
}

void ScanFilterOperator::reset()
{
}

bool ScanFilterOperator::nextRowId([[maybe_unused]] RowId* row_id)
{
    // // TODO: remove
    // if (iterator_.hasNext()) {
    //     iterator_.next();
    //     *row_id = iterator_.getCurrentRowId();
    //     return true;
    // }
    // return false;
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
