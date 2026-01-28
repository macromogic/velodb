#pragma once

#include "planner/abstract_plan_node.hpp"

namespace velodb {

// Materialization plan node: takes join rowid pairs + target schema and produces fully materialized rows
class MaterializationPlanNode : public AbstractPlanNode {
public:
    explicit MaterializationPlanNode(Schema output_schema)
        : AbstractPlanNode(PlanType::MATERIALIZATION, std::move(output_schema))
    {
    }
    ~MaterializationPlanNode() override = default;

    std::unique_ptr<AbstractOperator> createOperator(ExecutionContext& context) const override;
    std::string toString() const override { return "Materialization()"; }
};

} // namespace velodb
