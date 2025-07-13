#pragma once

#include "planner/abstract_plan_node.hpp"
#include "catalog/table.hpp"
#include "execution/expression.hpp"
#include <memory>
#include <string>

namespace velodb {

// Sequential scan plan node
class SeqScanPlanNode : public AbstractPlanNode {
public:
    explicit SeqScanPlanNode(const TableBase& table, std::unique_ptr<AbstractExpression> predicate = nullptr);
    ~SeqScanPlanNode() override = default;

    std::unique_ptr<AbstractOperator> createOperator(ExecutionContext* context) const override;
    [[nodiscard]] std::string toString() const override;

    [[nodiscard]] const TableBase& getTable() const { return table_; }
    [[nodiscard]] const AbstractExpression* getPredicate() const { return predicate_.get(); }

private:
    const TableBase& table_;
    std::unique_ptr<AbstractExpression> predicate_;
};

} // namespace velodb
