#pragma once

#include "catalog/table.hpp"
#include "expression/expression.hpp"

namespace velodb {

// Column reference expression
class ColumnRefExpression : public LeafExpression {
public:
    ColumnRefExpression(std::string table_name, std::string column_name, std::unique_ptr<DataType> return_type);
    ~ColumnRefExpression() override = default;

    const Value evaluate(const Tuple& tuple, const Schema& schema) const override;
    std::string toString() const override;

    const std::string& getTableName() const { return table_name_; }
    const std::string& getColumnName() const { return column_name_; }

protected:
    std::unique_ptr<AbstractExpression> cloneUniqueImpl() const override;

private:
    std::string table_name_;
    std::string column_name_;
};

} // namespace velodb
