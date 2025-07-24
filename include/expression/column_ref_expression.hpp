#pragma once

#include "expression.hpp"

namespace velodb {

// Column reference expression
class ColumnRefExpression : public AbstractExpression {
public:
    ColumnRefExpression(std::string column_name, std::unique_ptr<DataType> return_type);
    ColumnRefExpression(size_t column_index, std::unique_ptr<DataType> return_type);
    ~ColumnRefExpression() override = default;

    Value evaluate(const Tuple& tuple, const Schema& schema) const override;
    std::vector<size_t> getRequiredColumns(const Schema& schema) const override;
    std::string toString() const override;

    const std::string& getColumnName() const { return column_name_; }
    size_t getColumnIndex() const { return column_index_; }
    bool hasColumnIndex() const { return has_column_index_; }

private:
    std::string column_name_;
    size_t column_index_;
    bool has_column_index_;
};

} // namespace velodb
