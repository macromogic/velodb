#pragma once

#include "catalog/catalog.hpp"
#include "catalog/schema.hpp"
#include "common/copy_traits.hpp"
#include "operator/operator.hpp"

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
    COMPACTION,
    PROJECTION,
    NESTED_LOOP_JOIN,
    HASH_JOIN,
    MERGE_SORT_JOIN,
    SORT,
    LIMIT,
    AGGREGATE
};

// Abstract base class for plan nodes
class AbstractPlanNode : private NonCopyable {
public:
    AbstractPlanNode(PlanType type, std::unique_ptr<Schema> output_schema);
    virtual ~AbstractPlanNode() = default;

    PlanType getPlanType() const { return type_; }
    const Schema& getOutputSchema() const { return *output_schema_; }

    const std::vector<std::unique_ptr<AbstractPlanNode>>& getChildren() const { return children_; }
    void addChild(std::unique_ptr<AbstractPlanNode> child);

    // Convert plan to executable operator
    virtual std::unique_ptr<AbstractOperator> createOperator(ExecutionContext& context) const = 0;

    virtual std::string toString() const = 0;

protected:
    PlanType type_;
    std::unique_ptr<Schema> output_schema_;
    std::vector<std::unique_ptr<AbstractPlanNode>> children_;
};

} // namespace velodb
