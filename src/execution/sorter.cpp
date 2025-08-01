#include "execution/sorter.hpp"
#include "catalog/table.hpp"
#include "common/exception.hpp"
#include <algorithm>
#include <stdexcept>

namespace velodb {

// TODO: Implement efficient sorting for large datasets

Sorter::Sorter(const Schema& schema,
    std::vector<std::unique_ptr<AbstractExpression>> sort_expressions,
    std::vector<bool> ascending_flags)
    : schema_(schema)
    , sort_expressions_(std::move(sort_expressions))
    , ascending_flags_(std::move(ascending_flags))
{

    if (sort_expressions_.size() != ascending_flags_.size()) {
        VELODB_THROW(ExecutionError, "Sort expressions and ascending flags must have the same size");
    }
}

void Sorter::addTuple(const Tuple& tuple)
{
    tuples_.push_back(tuple);
    is_sorted_ = false;
}

void Sorter::addTuple(Tuple&& tuple)
{
    tuples_.push_back(std::move(tuple));
    is_sorted_ = false;
}

void Sorter::sort()
{
    if (is_sorted_ || tuples_.empty()) {
        return;
    }

    // For now, use in-memory sorting

    // Create sort keys for all tuples
    std::vector<SortKey> sort_keys;
    sort_keys.reserve(tuples_.size());

    for (size_t i = 0; i < tuples_.size(); ++i) {
        std::vector<Value> key_values = createSortKey(tuples_[i]);
        sort_keys.emplace_back(std::move(key_values), i);
    }

    // Sort the keys
    auto comparator = createComparator();
    std::sort(sort_keys.begin(), sort_keys.end(), comparator);

    // Reorder tuples based on sorted keys
    sorted_tuples_.clear();
    sorted_tuples_.reserve(tuples_.size());

    for (const auto& sort_key : sort_keys) {
        sorted_tuples_.push_back(tuples_[sort_key.tuple_index_]);
    }

    is_sorted_ = true;
}

void Sorter::clear()
{
    tuples_.clear();
    sorted_tuples_.clear();
    is_sorted_ = false;
}

std::vector<Value> Sorter::createSortKey(const Tuple& tuple) const
{
    std::vector<Value> key_values;
    key_values.reserve(sort_expressions_.size());

    for (const auto& expr : sort_expressions_) {
        Value const value = expr->evaluate(tuple, schema_);
        key_values.push_back(value);
    }

    return key_values;
}

SortComparator Sorter::createComparator() const
{
    return [this](const SortKey& left, const SortKey& right) -> bool {
        // Compare each sort key in order
        for (size_t i = 0; i < left.key_values_.size(); ++i) {
            const Value& left_val = left.key_values_[i];
            const Value& right_val = right.key_values_[i];
            bool const ascending = ascending_flags_[i];

            if (left_val == right_val) {
                continue; // Equal values, move to next key
            }
            return ascending ? (left_val < right_val) : (right_val < left_val);
        }

        // All keys are equal
        return false;
    };
}

} // namespace velodb
