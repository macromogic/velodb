#include "planner/plan_visualizer.hpp"
#include <sstream>
#include <iomanip>
#include <map>

namespace velodb {

// Static method implementations
std::string PlanVisualizer::visualizeAsText(const std::unique_ptr<AbstractPlanNode>& plan_node, int indent) {
    std::string result;
    visualizeTextRecursive(plan_node, result, indent);
    return result;
}

std::string PlanVisualizer::visualizeAsGraphviz(const std::unique_ptr<AbstractPlanNode>& plan_node, 
                                               const std::string& graph_name) {
    std::string result;
    result += "digraph " + graph_name + " {\n";
    result += "  rankdir=TB;\n";
    result += "  node [shape=box, style=filled, fontname=\"Arial\", fontsize=10];\n";
    result += "  edge [fontname=\"Arial\", fontsize=8];\n\n";
    
    int node_counter = 0;
    visualizeGraphvizRecursive(plan_node, result, node_counter);
    
    result += "}\n";
    return result;
}

std::string PlanVisualizer::visualizeDetailed(const std::unique_ptr<AbstractPlanNode>& plan_node) {
    std::string result;
    result += "=== Query Plan Analysis ===\n\n";
    visualizeDetailedRecursive(plan_node, result, 0);
    return result;
}

void PlanVisualizer::printPlan(const std::unique_ptr<AbstractPlanNode>& plan_node, 
                              std::ostream& out, 
                              OutputFormat format) {
    switch (format) {
        case OutputFormat::TEXT_TREE:
            out << visualizeAsText(plan_node);
            break;
        case OutputFormat::GRAPHVIZ_DOT:
            out << visualizeAsGraphviz(plan_node);
            break;
        case OutputFormat::DETAILED:
            out << visualizeDetailed(plan_node);
            break;
    }
}

// Private helper methods
void PlanVisualizer::visualizeTextRecursive(const std::unique_ptr<AbstractPlanNode>& plan_node, 
                                           std::string& result, int indent) {
    if (!plan_node) {
        return;
    }

    // Create indentation
    std::string indent_str(indent, ' ');
    result += indent_str;
    
    // Add tree structure symbols
    if (indent > 0) {
        result += "├── ";
    }
    
    // Add node information
    result += getNodeLabel(*plan_node);
    result += " [" + planTypeToString(plan_node->getPlanType()) + "]";
    
    // Add schema information
    const auto& schema = plan_node->getOutputSchema();
    result += " (cols: " + std::to_string(schema.getColumnCount()) + ")";
    
    result += "\n";

    // Recursively process children
    const auto& children = plan_node->getChildren();
    for (size_t i = 0; i < children.size(); ++i) {
        visualizeTextRecursive(children[i], result, indent + 4);
    }
}

void PlanVisualizer::visualizeGraphvizRecursive(const std::unique_ptr<AbstractPlanNode>& plan_node, 
                                               std::string& result, int& node_counter) {
    if (!plan_node) {
        return;
    }

    int current_node = node_counter++;
    
    // Create node
    result += "  node" + std::to_string(current_node) + " [";
    result += "label=\"" + escapeForDot(getNodeLabel(*plan_node)) + "\", ";
    result += "shape=" + getNodeShape(plan_node->getPlanType()) + ", ";
    result += "fillcolor=\"" + getNodeColor(plan_node->getPlanType()) + "\"";
    result += "];\n";

    // Process children and create edges
    const auto& children = plan_node->getChildren();
    for (const auto& child : children) {
        int child_node = node_counter;
        visualizeGraphvizRecursive(child, result, node_counter);
        
        // Create edge from current node to child
        result += "  node" + std::to_string(current_node) + " -> node" + std::to_string(child_node) + ";\n";
    }
}

void PlanVisualizer::visualizeDetailedRecursive(const std::unique_ptr<AbstractPlanNode>& plan_node, 
                                               std::string& result, int level) {
    if (!plan_node) {
        return;
    }

    // Create level indicator
    std::string prefix(level * 2, ' ');
    result += prefix + "Level " + std::to_string(level) + ": ";
    
    // Node type and basic info
    result += planTypeToString(plan_node->getPlanType()) + "\n";
    result += prefix + "  Description: " + plan_node->toString() + "\n";
    
    // Schema details
    const auto& schema = plan_node->getOutputSchema();
    result += prefix + "  Output Schema:\n";
    result += prefix + "    Column Count: " + std::to_string(schema.getColumnCount()) + "\n";
    
    for (size_t i = 0; i < schema.getColumnCount(); ++i) {
        const auto& column = schema.getColumn(i);
        result += prefix + "    [" + std::to_string(i) + "] " + column.getName() + 
                 " (" + column.getType().toString() + ")\n";
    }
    
    // Children info
    const auto& children = plan_node->getChildren();
    if (!children.empty()) {
        result += prefix + "  Children: " + std::to_string(children.size()) + "\n";
        for (size_t i = 0; i < children.size(); ++i) {
            result += prefix + "  Child " + std::to_string(i) + ":\n";
            visualizeDetailedRecursive(children[i], result, level + 1);
        }
    } else {
        result += prefix + "  Children: None (leaf node)\n";
    }
    
    result += "\n";
}

std::string PlanVisualizer::planTypeToString(PlanType type) {
    switch (type) {
        case PlanType::SCAN_FILTER: return "Scan Filter";
        case PlanType::PROJECTION: return "Projection";
        case PlanType::NESTED_LOOP_JOIN: return "Nested Loop Join";
        case PlanType::HASH_JOIN: return "Hash Join";
        case PlanType::MERGE_SORT_JOIN: return "Merge Sort Join";
        case PlanType::SORT: return "Sort";
        case PlanType::LIMIT: return "Limit";
        case PlanType::AGGREGATE: return "Aggregate";
        case PlanType::INVALID: return "Invalid";
        default: return "Unknown";
    }
}

std::string PlanVisualizer::getNodeLabel(const AbstractPlanNode& node) {
    return node.toString();
}

std::string PlanVisualizer::getNodeShape(PlanType type) {
    switch (type) {
        case PlanType::SCAN_FILTER: return "diamond";
        case PlanType::PROJECTION: return "ellipse";
        case PlanType::NESTED_LOOP_JOIN:
        case PlanType::HASH_JOIN:
        case PlanType::MERGE_SORT_JOIN: return "hexagon";
        case PlanType::SORT: return "parallelogram";
        case PlanType::LIMIT: return "trapezium";
        case PlanType::AGGREGATE: return "octagon";
        default: return "box";
    }
}

std::string PlanVisualizer::getNodeColor(PlanType type) {
    switch (type) {
        case PlanType::SCAN_FILTER: return "yellow";
        case PlanType::PROJECTION: return "lightgreen";
        case PlanType::NESTED_LOOP_JOIN:
        case PlanType::HASH_JOIN:
        case PlanType::MERGE_SORT_JOIN: return "orange";
        case PlanType::SORT: return "purple";
        case PlanType::LIMIT: return "pink";
        case PlanType::AGGREGATE: return "red";
        default: return "white";
    }
}

std::string PlanVisualizer::escapeForDot(const std::string& str) {
    std::string result = str;
    
    // Replace special characters for DOT format
    size_t pos = 0;
    while ((pos = result.find('"', pos)) != std::string::npos) {
        result.replace(pos, 1, "\\\"");
        pos += 2;
    }
    
    pos = 0;
    while ((pos = result.find('\\', pos)) != std::string::npos) {
        result.replace(pos, 1, "\\\\");
        pos += 2;
    }
    
    return result;
}

} // namespace velodb
