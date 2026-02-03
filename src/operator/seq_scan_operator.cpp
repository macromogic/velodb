#include "operator/seq_scan_operator.hpp"

#include "common/constants.hpp"
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
    PROFILE_SCOPE("SeqScan");
    size_t start_row_id = current_row_id_;
    size_t end_row_id = std::min(start_row_id + MAX_BATCH_SIZE, table_.getRowCount());
    size_t batch_size = end_row_id - start_row_id;
    RowBatch batch;
    {
        batch = table_.slice(start_row_id, end_row_id);
    }
    Column rowids_(DataType::createType(DataTypeId::BIGINT), batch_size);
    Column masks_(DataType::createType(DataTypeId::BOOLEAN), batch_size);

    if (predicate_) {
        masks_ = predicate_->evaluateBatch(batch, output_schema_);
    } else {
        // Fast path: directly fill the mask buffer with 1s
        uint8_t* mask_data = static_cast<uint8_t*>(masks_.rawData());
        std::fill_n(mask_data, batch_size, static_cast<uint8_t>(1));
        masks_.setSize(batch_size);
    }

    {
        // Fast path: directly write to the rowids buffer
        int64_t* rowid_data = static_cast<int64_t*>(rowids_.rawData());
        for (size_t i = 0; i < batch_size; ++i) {
            rowid_data[i] = static_cast<int64_t>(current_row_id_ + i);
        }
        rowids_.setSize(batch_size);
        current_row_id_ = end_row_id;
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
