#include "catalog/tuple.hpp"

#include "catalog/row_batch.hpp"
#include "catalog/table.hpp"
#include "common/exception.hpp"
#include "common/fmt.hpp"

#include <fmt/core.h>
#include <fmt/ranges.h>

namespace velodb {

ValueTuple::ValueTuple(const Schema& schema)
    : schema_(std::cref(schema))
{
    values_.resize(schema.getColumnCount());
    for (size_t i = 0; i < schema.getColumnCount(); ++i) {
        values_[i] = Value::createNull(schema.getColumnInfo(i).getType().getTypeId());
    }
}

ValueTuple::ValueTuple(const Schema& schema, std::vector<Value> values)
    : schema_(std::cref(schema))
    , values_(std::move(values))
{
    if (values_.size() != schema.getColumnCount()) {
        VELODB_THROW(CatalogError, "Value count mismatch with schema");
    }
}

const Value ValueTuple::getValue(size_t column_index) const
{
    if (column_index >= values_.size()) {
        VELODB_THROW(CatalogError, "Column index out of range");
    }
    return values_[column_index];
}

// const Value ValueTuple::getValue(const std::string& column_name) const
// {
//     size_t const index = schema_.get().getColumnIndex(column_name);
//     return values_[index];
// }

std::string ValueTuple::toString() const
{
    return fmt::format("({})", fmt::join(values_, ", "));
}

ViewTuple::ViewTuple(const RowBatch& batch, size_t row_id)
    : batch_(&batch)
    , row_id_(row_id)
{
    if (batch_ == nullptr) {
        VELODB_THROW(CatalogError, "Batch pointer is null");
    }
}

bool ViewTuple::operator==(const ViewTuple& other) const
{
    return batch_ == other.batch_ && row_id_ == other.row_id_;
}

const Value ViewTuple::getValue(size_t column_index) const
{
    if (column_index >= batch_->getColumnCount()) {
        VELODB_THROW(CatalogError, "Column index out of range");
    }
    return batch_->getValue(row_id_, column_index);
}

// const Value ViewTuple::getValue(const std::string& column_name) const
// {
//     size_t const index = batch_.get().getSchema().getColumnIndex(column_name);
//     return getValue(index);
// }

size_t ViewTuple::getColumnCount() const
{
    return batch_->getColumnCount();
}

std::string ViewTuple::toString() const
{
    std::string result = "(";
    for (size_t i = 0; i < getColumnCount(); ++i) {
        if (i > 0)
            result += ", ";
        result += getValue(i).toString();
    }
    result += ")";
    return result;
}

void ViewTuple::setBatch(const RowBatch& batch, size_t row_id)
{
    batch_ = &batch;
    row_id_ = row_id;
}

} // namespace velodb
