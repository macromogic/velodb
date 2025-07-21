#pragma once

#include "planner/abstract_plan_node.hpp"
#include "catalog/table.hpp"
#include "execution/expression.hpp"
#include <memory>
#include <string>

namespace velodb {

// Scan with filter plan node
class ScanFilterPlanNode : public AbstractPlanNode {
public:
    explicit ScanFilterPlanNode(const TableBase& table, std::unique_ptr<AbstractExpression> predicate = nullptr);
    ~ScanFilterPlanNode() override = default;

    std::unique_ptr<AbstractOperator> createOperator(ExecutionContext* context) const override;
    [[nodiscard]] std::string toString() const override;

    [[nodiscard]] const TableBase& getTable() const { return table_; }
    [[nodiscard]] const AbstractExpression* getPredicate() const { return predicate_.get(); }

private:
    const TableBase& table_;
    std::unique_ptr<AbstractExpression> predicate_;
};

} // namespace velodb
