#pragma once

#include "schema.hpp"

#include "data/value.hpp"

namespace velodb {

// forward declarations
class TableBase;

class Tuple {
public:
    virtual ~Tuple() = default;

    virtual const Value getValue(size_t column_index) const = 0;
    virtual const Value getValue(const std::string& column_name) const = 0;
    virtual size_t getColumnCount() const = 0;
    virtual std::string toString() const = 0;
};

class ValueTuple : public Tuple {
public:
    explicit ValueTuple(const Schema& schema);
    ValueTuple(const Schema& schema, std::vector<Value> values);
    ~ValueTuple() = default;

    // Copy and move constructors
    ValueTuple(const ValueTuple& other) = default;
    ValueTuple(ValueTuple&& other) noexcept = default;
    ValueTuple& operator=(const ValueTuple& other) = default;
    ValueTuple& operator=(ValueTuple&& other) noexcept = default;

    const Value getValue(size_t column_index) const;
    const Value getValue(const std::string& column_name) const;

    const Schema& getSchema() const { return schema_.get(); }
    size_t getColumnCount() const override { return values_.size(); }

    std::string toString() const override;

private:
    std::reference_wrapper<const Schema> schema_;
    std::vector<Value> values_;
};

class ViewTuple : public Tuple {
public:
    ViewTuple(const TableBase& table, size_t row_id);
    ~ViewTuple() = default;

    bool operator==(const ViewTuple& other) const;

    const Value getValue(size_t column_index) const override;
    const Value getValue(const std::string& column_name) const override;
    size_t getColumnCount() const override;
    std::string toString() const override;

private:
    void setTable(const TableBase& table, size_t row_id = 0);

    std::reference_wrapper<const TableBase> table_;
    size_t row_id_;

    friend class TableIterator;
    friend class QueryResultIterator;
};

} // namespace velodb
