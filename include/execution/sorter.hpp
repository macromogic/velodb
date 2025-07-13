#pragma once

#include "catalog/schema.hpp"
#include "execution/expression.hpp"
#include "types/value.hpp"
#include <functional>
#include <memory>
#include <vector>

namespace velodb {

// Forward declarations
class Tuple;

// Sort key for a tuple
struct SortKey {
    std::vector<Value> key_values_;
    size_t tuple_index_; // Index into the original tuple vector

    SortKey(std::vector<Value> keys, size_t index)
        : key_values_(std::move(keys))
        , tuple_index_(index)
    {
    }
};

// Comparison function for sort keys
using SortComparator = std::function<bool(const SortKey&, const SortKey&)>;

// Sorter interface for sorting tuples
class Sorter {
public:
    Sorter(const Schema& schema,
        std::vector<std::unique_ptr<AbstractExpression>> sort_expressions,
        std::vector<bool> ascending_flags);
    ~Sorter() = default;

    // Delete copy constructor and assignment
    Sorter(const Sorter&) = delete;
    Sorter& operator=(const Sorter&) = delete;

    // Add a tuple to be sorted
    void addTuple(const Tuple& tuple);
    void addTuple(Tuple&& tuple);

    // Sort all added tuples
    void sort();

    // Get sorted tuples
    [[nodiscard]] const std::vector<Tuple>& getSortedTuples() const { return sorted_tuples_; }

    // Iterator interface for sorted tuples
    class Iterator {
    public:
        Iterator(const std::vector<Tuple>& tuples, size_t index)
            : tuples_(tuples)
            , index_(index)
        {
        }

        [[nodiscard]] bool hasNext() const { return index_ < tuples_.size(); }
        const Tuple& next() { return tuples_[index_++]; }
        void reset() { index_ = 0; }

    private:
        const std::vector<Tuple>& tuples_;
        size_t index_;
    };

    // Get iterator for sorted tuples
    [[nodiscard]] std::unique_ptr<Iterator> getIterator() const;

    // Statistics
    [[nodiscard]] size_t getSize() const { return tuples_.size(); }
    [[nodiscard]] bool isEmpty() const { return tuples_.empty(); }
    [[nodiscard]] bool isSorted() const { return is_sorted_; }

    // Clear all tuples
    void clear();

private:
    const Schema& schema_;
    std::vector<std::unique_ptr<AbstractExpression>> sort_expressions_;
    std::vector<bool> ascending_flags_;

    std::vector<Tuple> tuples_; // Original tuples
    std::vector<Tuple> sorted_tuples_; // Sorted tuples
    bool is_sorted_ { false };

    // Create sort keys for a tuple
    [[nodiscard]] std::vector<Value> createSortKey(const Tuple& tuple) const;

    // Create comparator function
    [[nodiscard]] SortComparator createComparator() const;

    // TODO: Implement external sorting for large datasets
    // TODO: Implement different sorting algorithms (quicksort, mergesort, etc.)
};

} // namespace velodb
