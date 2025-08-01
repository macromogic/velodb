#pragma once

#include "catalog/catalog.hpp"
#include "planner/abstract_plan_node.hpp"
#include <memory>
#include <ostream>
#include <string>

namespace velodb {

class PlanVisualizer {
public:
    enum class OutputFormat {
        TEXT_TREE, // Hierarchical text format
        GRAPHVIZ_DOT, // DOT format for Graphviz
        DETAILED // Detailed analysis with statistics
    };

    static std::string visualizeAsText(const std::unique_ptr<AbstractPlanNode>& plan_node, int indent = 0);
    static std::string visualizeAsGraphviz(const std::unique_ptr<AbstractPlanNode>& plan_node,
        const std::string& graph_name = "QueryPlan");
    static std::string visualizeDetailed(const std::unique_ptr<AbstractPlanNode>& plan_node);
    static void printPlan(const std::unique_ptr<AbstractPlanNode>& plan_node,
        std::ostream& out,
        OutputFormat format = OutputFormat::TEXT_TREE);

private:
    // Helper methods for different visualization formats
    static void visualizeTextRecursive(const std::unique_ptr<AbstractPlanNode>& plan_node,
        std::string& result, int indent);

    static void visualizeGraphvizRecursive(const std::unique_ptr<AbstractPlanNode>& plan_node,
        std::string& result, int& node_counter);

    static void visualizeDetailedRecursive(const std::unique_ptr<AbstractPlanNode>& plan_node,
        std::string& result, int level);

    static std::string planTypeToString(PlanType type);
    static std::string getNodeLabel(const AbstractPlanNode& node);
    static std::string getNodeShape(PlanType type);
    static std::string getNodeColor(PlanType type);
    static std::string escapeForDot(const std::string& str);
};

} // namespace velodb
