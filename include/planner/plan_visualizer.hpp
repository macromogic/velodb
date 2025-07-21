#pragma once

#include "planner/abstract_plan_node.hpp"
#include "catalog/catalog.hpp"
#include <memory>
#include <string>
#include <ostream>

namespace velodb {

/**
 * @brief Utility class for visualizing query execution plans
 * 
 * Provides multiple output formats for plan visualization:
 * - Text-based tree format (default)
 * - Graphviz DOT format for graph visualization
 * - Detailed analysis format with statistics
 */
class PlanVisualizer {
public:
    enum class OutputFormat {
        TEXT_TREE,      // Hierarchical text format
        GRAPHVIZ_DOT,   // DOT format for Graphviz
        DETAILED        // Detailed analysis with statistics
    };

    /**
     * @brief Visualize a query plan in text format
     * @param plan_node Root node of the plan tree
     * @param indent Initial indentation level
     * @return String representation of the plan
     */
    static std::string visualizeAsText(const std::unique_ptr<AbstractPlanNode>& plan_node, int indent = 0);

    /**
     * @brief Visualize a query plan as Graphviz DOT format
     * @param plan_node Root node of the plan tree
     * @param graph_name Name of the graph (default: "QueryPlan")
     * @return DOT format string for Graphviz
     */
    static std::string visualizeAsGraphviz(const std::unique_ptr<AbstractPlanNode>& plan_node, 
                                         const std::string& graph_name = "QueryPlan");

    /**
     * @brief Visualize a query plan with detailed analysis
     * @param plan_node Root node of the plan tree
     * @return Detailed analysis string
     */
    static std::string visualizeDetailed(const std::unique_ptr<AbstractPlanNode>& plan_node);

    /**
     * @brief Print plan to output stream
     * @param plan_node Root node of the plan tree
     * @param out Output stream
     * @param format Output format
     */
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
