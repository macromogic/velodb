#include "execution/query_result.hpp"

#include "common/exception.hpp"
#include "common/fmt.hpp"

#include <fmt/format.h>

namespace velodb {

QueryResult::QueryResult(std::unique_ptr<Schema> schema)
    : schema_(std::move(schema))
    , row_count_(0)
{
    views_.emplace_back(""); // dummy view to simplify iterator logic
}

void QueryResult::append(View view)
{
    views_.push_back(std::move(view));
    row_offsets_.push_back(row_count_);
    row_count_ += views_.back().getRowCount();
}

Value QueryResult::getValue(size_t row, size_t column) const
{
    // position 0 reserved for end iterator
    auto index = std::upper_bound(row_offsets_.begin(), row_offsets_.end(), row) - row_offsets_.begin();
    const auto& view = views_[index];
    size_t local_row = row - row_offsets_[index];
    if (local_row >= view.getRowCount()) {
        VELODB_THROW(CatalogError, "Row index out of range");
    }
    if (column >= view.getSchema().getColumnCount()) {
        VELODB_THROW(CatalogError, "Column index out of range");
    }
    return view.getValue(local_row, column);
}

std::string QueryResult::toString() const
{
    return fmt::format("QueryResult: {} rows\nSchema: {}", row_count_, *schema_);
}

QueryResultIterator QueryResult::begin() const
{
    if (row_offsets_.empty()) {
        return QueryResultIterator(*this, 0, 0);
    }
    return QueryResultIterator(*this, 1, 0);
}

QueryResultIterator QueryResult::end() const
{
    return QueryResultIterator(*this, 0, 0);
}

// QueryResultIterator implementation
QueryResultIterator::QueryResultIterator(const QueryResult& result, size_t view_idx, size_t row_id)
    : result_(result)
    , view_idx_(view_idx)
    , current_tuple_(result_.views_[view_idx_], row_id)
{
}

bool QueryResultIterator::operator==(const QueryResultIterator& other) const
{
    return &result_ == &other.result_ && current_tuple_ == other.current_tuple_;
}

bool QueryResultIterator::operator!=(const QueryResultIterator& other) const
{
    return !(*this == other);
}

QueryResultIterator& QueryResultIterator::operator++()
{
    if (view_idx_ == 0) {
        return *this; // Already at end
    }
    ++current_tuple_.row_id_;
    if (current_tuple_.row_id_ >= result_.views_[view_idx_].getRowCount()) {
        // Move to next view
        ++view_idx_;
        if (view_idx_ >= result_.views_.size()) {
            view_idx_ = 0; // point to dummy view
        }
        current_tuple_.setTable(result_.views_[view_idx_]);
    }
    return *this;
}

QueryResultIterator QueryResultIterator::operator++(int)
{
    QueryResultIterator temp = *this;
    operator++();
    return temp;
}

const ViewTuple& QueryResultIterator::operator*() const
{

    return current_tuple_;
}

const ViewTuple* QueryResultIterator::operator->() const
{
    return &current_tuple_;
}

} // namespace velodb
