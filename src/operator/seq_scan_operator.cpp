#include "operator/seq_scan_operator.hpp"

#include "common/fmt.hpp"
#include "execution/execution_engine.hpp"
#include "expression/expression.hpp"

#include <stdexcept>

namespace velodb {

SeqScanOperator::SeqScanOperator(ExecutionContext& context,
                                 const Table& table,
                                 Schema output_schema,
                                 const std::unique_ptr<AbstractExpression>& predicate)
    : UnaryOperator(context,
                    std::move(output_schema),
                    nullptr) // May support child operators in future
    , table_(table)
    , predicate_(predicate)
    , current_row_id_(0)
{
}

Result<RowBatch> SeqScanOperator::next()
{
    PROFILE_SCOPE("SeqScanOperator::next");
    size_t start_row_id = current_row_id_;
    size_t end_row_id = std::min(start_row_id + MAX_BATCH_SIZE, table_.getRowCount());
    size_t batch_size = end_row_id - start_row_id;
    auto batch = table_.slice(start_row_id, end_row_id);
    Column rowids_(DataType::createType(DataTypeId::BIGINT), batch_size);
    Column masks_(DataType::createType(DataTypeId::BOOLEAN), batch_size);
    for (const auto& tuple : batch) {
        Value result = predicate_ ? predicate_->evaluate(tuple, output_schema_) : Value::createBoolean(true);
        rowids_.append(Value::createBigInt(current_row_id_));
        masks_.append(result);
        ++current_row_id_;
    }
    batch.addColumn(std::move(rowids_));
    batch.addColumn(std::move(masks_));
    return Result<RowBatch>::success(std::move(batch));
}

std::string SeqScanOperator::toString() const
{
    std::string result = fmt::format("SeqScanOperator({})", table_.getName());
    if (predicate_) {
        result += fmt::format(" WHERE {}", *predicate_);
    }
    return result;
}

} // namespace velodb
