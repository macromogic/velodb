#include "operator/result_set.hpp"

namespace velodb {

// TODO: Implement full operator execution system with late materialization

// ResultSet implementation
ResultSet::ResultSet(std::unique_ptr<Schema> schema)
    : schema_(std::move(schema))
{
}

void ResultSet::addTuple(const Tuple& tuple)
{
    tuples_.push_back(tuple);
}

void ResultSet::addTuple(Tuple&& tuple)
{
    tuples_.push_back(std::move(tuple));
}

void ResultSet::addRowId(RowId row_id, const TableBase& table)
{
    row_ids_.push_back(row_id);
    source_table_ = &table;
}

void ResultSet::clear()
{
    tuples_.clear();
    row_ids_.clear();
    source_table_ = nullptr;
}

} // namespace velodb
