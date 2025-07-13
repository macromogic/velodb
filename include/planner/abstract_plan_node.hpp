#pragma once

#include "catalog/schema.hpp"
#include "execution/operator.hpp"
#include <memory>
#include <string>
#include <vector>

namespace velodb {

// Forward declarations
class ExecutionContext;

// Plan node types
enum class PlanType {
    INVALID = 0,
    SEQ_SCAN,
    PROJECTION,
    FILTER,
    NESTED_LOOP_JOIN,
    HASH_JOIN,
    MERGE_SORT_JOIN,
    SORT,
    LIMIT,
    AGGREGATE
};

// Abstract base class for plan nodes
class AbstractPlanNode {
public:
    AbstractPlanNode(PlanType type, std::unique_ptr<Schema> output_schema);
    virtual ~AbstractPlanNode() = default;

    // Delete copy constructor and assignment
    AbstractPlanNode(const AbstractPlanNode&) = delete;
    AbstractPlanNode& operator=(const AbstractPlanNode&) = delete;

    [[nodiscard]] PlanType getPlanType() const { return type_; }
    [[nodiscard]] const Schema& getOutputSchema() const { return *output_schema_; }

    [[nodiscard]] const std::vector<std::unique_ptr<AbstractPlanNode>>& getChildren() const { return children_; }
    void addChild(std::unique_ptr<AbstractPlanNode> child);

    // Convert plan to executable operator
    virtual std::unique_ptr<AbstractOperator> createOperator(ExecutionContext* context) const = 0;

    [[nodiscard]] virtual std::string toString() const = 0;

protected:
    PlanType type_;
    std::unique_ptr<Schema> output_schema_;
    std::vector<std::unique_ptr<AbstractPlanNode>> children_;
};

} // namespace velodb
