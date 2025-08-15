#pragma once

#include "catalog/table.hpp"
#include "expression/expression.hpp"
#include "planner/abstract_plan_node.hpp"

#include <memory>
#include <string>

namespace velodb {

// Scan with filter plan node
class SeqScanPlanNode : public AbstractPlanNode {
public:
    explicit SeqScanPlanNode(const TableBase& table,
                             std::unique_ptr<Schema> output_schema,
                             std::unique_ptr<AbstractExpression> predicate = nullptr);
    ~SeqScanPlanNode() override = default;

    std::unique_ptr<AbstractOperator> createOperator(ExecutionContext& context) const override;
    std::string toString() const override;

    const TableBase& getTable() const { return table_; }
    const AbstractExpression* getPredicate() const { return predicate_.get(); }

private:
    const TableBase& table_;
    std::unique_ptr<AbstractExpression> predicate_;
};

} // namespace velodb
