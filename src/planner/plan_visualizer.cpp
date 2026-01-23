#include "planner/plan_visualizer.hpp"

#include "common/fmt.hpp"

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <iomanip>
#include <map>
#include <ostream>

namespace velodb {

auto format_as(PlanType type)
{
    switch (type) {
    case PlanType::SEQ_SCAN:
        return "Scan Filter";
    case PlanType::COMPACTION:
        return "FilterCompaction";
    case PlanType::PROJECTION:
        return "Projection";
    case PlanType::NESTED_LOOP_JOIN:
        return "Nested Loop Join";
    case PlanType::HASH_JOIN:
        return "Hash Join";
    case PlanType::SORT_MERGE_JOIN:
        return "Merge Sort Join";
    case PlanType::SORT:
        return "Sort";
    case PlanType::LIMIT:
        return "Limit";
    case PlanType::AGGREGATE:
        return "Aggregate";
    case PlanType::INVALID:
        return "Invalid";
    default:
        return "Unknown";
    }
}

// Static method implementations
std::string PlanVisualizer::visualizeAsText(const std::unique_ptr<AbstractPlanNode>& plan_node, int indent)
{
    std::ostringstream oss;
    visualizeTextRecursive(plan_node, oss, indent);
    return oss.str();
}

std::string PlanVisualizer::visualizeAsGraphviz(const std::unique_ptr<AbstractPlanNode>& plan_node,
                                                const std::string& graph_name)
{
    std::ostringstream oss;
    fmt::print(oss,
               "digraph {} {{\n"
               "  rankdir=TB;\n"
               "  node [shape=box, style=filled, fontname=\"Arial\", fontsize=10];\n"
               "  edge [fontname=\"Arial\", fontsize=8];\n\n",
               graph_name);

    int node_counter = 0;
    visualizeGraphvizRecursive(plan_node, oss, node_counter);

    oss << "}\n";
    return oss.str();
}

std::string PlanVisualizer::visualizeDetailed(const std::unique_ptr<AbstractPlanNode>& plan_node)
{
    std::ostringstream oss;
    oss << "=== Query Plan Analysis ===\n\n";
    visualizeDetailedRecursive(plan_node, oss, 0);
    return oss.str();
}

void PlanVisualizer::printPlan(const std::unique_ptr<AbstractPlanNode>& plan_node,
                               std::ostream& out,
                               OutputFormat format)
{
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
                                            std::ostringstream& oss,
                                            int indent)
{
    if (!plan_node) {
        return;
    }

    // Create indentation
    std::string indent_str(indent, ' ');
    oss << indent_str;

    // Add tree structure symbols
    if (indent > 0) {
        oss << "├── ";
    }

    // Add node information
    fmt::print(oss,
               "{} [{}] (cols: {})\n",
               *plan_node,
               plan_node->getPlanType(),
               plan_node->getOutputSchema().getColumnCount());

    // Recursively process children
    const auto& children = plan_node->getChildren();
    for (size_t i = 0; i < children.size(); ++i) {
        visualizeTextRecursive(children[i], oss, indent + 4);
    }
}

void PlanVisualizer::visualizeGraphvizRecursive(const std::unique_ptr<AbstractPlanNode>& plan_node,
                                                std::ostringstream& oss,
                                                int& node_counter)
{
    if (!plan_node) {
        return;
    }

    int current_node = node_counter++;

    // Create node
    auto plan_type = plan_node->getPlanType();
    fmt::print(oss,
               "  node {} [label=\"{}\", shape={}, fillcolor=\"{}\"];\n",
               current_node,
               escapeForDot(plan_node->toString()),
               getNodeShape(plan_type),
               getNodeColor(plan_type));

    // Process children and create edges
    const auto& children = plan_node->getChildren();
    for (const auto& child : children) {
        int child_node = node_counter;
        visualizeGraphvizRecursive(child, oss, node_counter);

        // Create edge from current node to child
        fmt::print(oss, "  node {} -> node {};\n", current_node, child_node);
    }
}

void PlanVisualizer::visualizeDetailedRecursive(const std::unique_ptr<AbstractPlanNode>& plan_node,
                                                std::ostringstream& oss,
                                                int level)
{
    if (!plan_node) {
        return;
    }

    // Create level indicator
    std::string prefix(level * 2, ' ');
    const auto& schema = plan_node->getOutputSchema();
    fmt::print(oss, "{}Level {}: {}\n", prefix, level, format_as(plan_node->getPlanType()));
    fmt::print(oss, "{}  Description: {}\n", prefix, plan_node->toString());
    fmt::print(oss, "{}  Output Schema:\n", prefix);
    fmt::print(oss, "{}    Column Count: {}\n", prefix, schema.getColumnCount());

    for (size_t i = 0; i < schema.getColumnCount(); ++i) {
        const auto& column = schema.getColumnInfo(i);
        fmt::print(oss, "{}    [{}] {} ({})\n", prefix, i, column.getName(), column.getType());
    }

    // Children info
    const auto& children = plan_node->getChildren();
    if (!children.empty()) {
        fmt::print(oss, "{}  Children: {}\n", prefix, children.size());
        for (size_t i = 0; i < children.size(); ++i) {
            fmt::print(oss, "{}  Child {}:\n", prefix, i);
            visualizeDetailedRecursive(children[i], oss, level + 1);
        }
    } else {
        fmt::print(oss, "{}  Children: None (leaf node)\n", prefix);
    }

    oss << "\n";
}

std::string PlanVisualizer::getNodeShape(PlanType type)
{
    switch (type) {
    case PlanType::SEQ_SCAN:
        return "diamond";
    case PlanType::PROJECTION:
        return "ellipse";
    case PlanType::NESTED_LOOP_JOIN:
    case PlanType::HASH_JOIN:
    case PlanType::SORT_MERGE_JOIN:
        return "hexagon";
    case PlanType::SORT:
        return "parallelogram";
    case PlanType::LIMIT:
        return "trapezium";
    case PlanType::AGGREGATE:
        return "octagon";
    default:
        return "box";
    }
}

std::string PlanVisualizer::getNodeColor(PlanType type)
{
    switch (type) {
    case PlanType::SEQ_SCAN:
        return "yellow";
    case PlanType::PROJECTION:
        return "lightgreen";
    case PlanType::NESTED_LOOP_JOIN:
    case PlanType::HASH_JOIN:
    case PlanType::SORT_MERGE_JOIN:
        return "orange";
    case PlanType::SORT:
        return "purple";
    case PlanType::LIMIT:
        return "pink";
    case PlanType::AGGREGATE:
        return "red";
    default:
        return "white";
    }
}

std::string PlanVisualizer::escapeForDot(const std::string& str)
{
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
