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
// clang-format off
enum class PlanType : uint16_t {
    INVALID          = 0,
    SEQ_SCAN         = 0b0000'0001,
    COMPACTION       = 0b0000'0010,
    PROJECTION       = 0b0000'0100,
    JOIN             = 0b0000'1000,
    NESTED_LOOP_JOIN = 0b0001'1000,
    HASH_JOIN        = 0b0010'1000,
    SORT_MERGE_JOIN  = 0b0011'1000,
    MATERIALIZATION  = 0b0100'0000,
    SORT             = 0b1000'0000,
    LIMIT          = 0b1'1000'0000,
    AGGREGATE     = 0b10'1000'0000
};
// clang-format on

// Abstract base class for plan nodes
class AbstractPlanNode : private NonCopyable {
public:
    AbstractPlanNode(PlanType type, Schema output_schema);
    virtual ~AbstractPlanNode() = default;

    PlanType getPlanType() const { return type_; }
    const Schema& getOutputSchema() const { return output_schema_; }

    const std::vector<std::unique_ptr<AbstractPlanNode>>& getChildren() const { return children_; }
    void addChild(std::unique_ptr<AbstractPlanNode> child);

    // Convert plan to executable operator
    virtual std::unique_ptr<AbstractOperator> createOperator(ExecutionContext& context) const = 0;

    virtual std::string toString() const = 0;

protected:
    PlanType type_;
    Schema output_schema_;
    std::vector<std::unique_ptr<AbstractPlanNode>> children_;
};

} // namespace velodb
