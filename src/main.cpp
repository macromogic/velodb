#include "velodb.hpp"

#include "catalog/mock_catalog_builder.hpp"
#include "catalog/tpch_catalog_builder.hpp"
#include "common/exception.hpp"
#include "planner/join_strategy.hpp"
#include "planner/plan_visualizer.hpp"

#include <argparse/argparse.hpp>

#include <iostream>

using namespace velodb;

int main(int argc, char* argv[])
{
    // Simple test version with mock catalog support
    argparse::ArgumentParser program("velodb", VERSION_STRING);

    program.add_argument("query").help("SQL query to execute");

    program.add_argument("--mock-catalog").help("Use mock catalog with adaptive table creation").flag();

    program.add_argument("--tpch-catalog").help("Use TPC-H catalog").flag();

    program.add_argument("--plan-format")
        .help("Format for query plan visualization {text,graphviz,detailed}")
        .default_value("text")
        .nargs(1)
        .choices("text", "graphviz", "detailed");

    program.add_argument("--join-strategy", "-j")
        .help("Join strategy to use {sort_merge, hash}")
        .default_value(std::string("sort_merge"))
        .choices("sort_merge", "sort-merge", "smj", "hash", "hj");

    program.add_argument("--verbose").help("Enable verbose output").flag();

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        std::cerr << "Error: " << err.what() << std::endl;
        std::cerr << program;
        return -1;
    }

    bool verbose = program.get<bool>("--verbose");
    bool use_mock_catalog = program.get<bool>("--mock-catalog");
    bool use_tpch_catalog = program.get<bool>("--tpch-catalog");
    JoinStrategy join_strategy = parseJoinStrategy(program.get<std::string>("--join-strategy"));

    if (verbose) {
        std::cout << "VelODB v" << VERSION_STRING << std::endl;
        std::cout << "Query Plan Visualization Tool" << std::endl;
        std::cout << "Join Strategy: " << joinStrategyToString(join_strategy) << std::endl;
        std::cout << "=============================" << std::endl;
    }

    std::string query = program.get<std::string>("query");

    if (use_mock_catalog || use_tpch_catalog) {
        std::optional<Catalog> catalog_opt;

        if (use_tpch_catalog) {
            catalog_opt = TPCHCatalogBuilder::createTPCHCatalog();
            if (verbose) {
                std::cout << "Created TPC-H catalog" << std::endl;
            }
        } else {
            // Create adaptive catalog and plan query
            auto catalog = MockCatalogBuilder::createAdaptiveCatalog();

            // Ensure tables exist for this query
            bool success = MockCatalogBuilder::ensureTablesForQuery(catalog, query);
            if (!success) {
                std::cerr << "Failed to create mock tables for query: " << query << std::endl;
                return -1;
            }

            if (verbose) {
                std::cout << "Created " << catalog.getTableCount() << " mock tables for query" << std::endl;
            }
            catalog_opt = std::move(catalog);
        }

        Catalog& catalog = *catalog_opt;

        // Plan and visualize
        hsql::SQLParserResult result;
        hsql::SQLParser::parse(query, &result);

        if (!result.isValid()) {
            std::cerr << "Invalid SQL query: " << result.errorMsg() << std::endl;
            return -1;
        }

        if (result.getStatement(0)->type() == hsql::kStmtSelect) {
            QueryPlanner planner(catalog, join_strategy);
            const auto* select_stmt = static_cast<const hsql::SelectStatement*>(result.getStatement(0));

            try {
                auto plan = planner.planSelect(select_stmt);

                std::cout << "Query plan for: " << query << std::endl;
                std::cout << std::endl;

                auto format = program.get<std::string>("--plan-format");
                if (format == "text") {
                    std::cout << PlanVisualizer::visualizeAsText(plan) << std::endl;
                } else if (format == "graphviz") {
                    std::cout << PlanVisualizer::visualizeAsGraphviz(plan) << std::endl;
                } else if (format == "detailed") {
                    std::cout << PlanVisualizer::visualizeDetailed(plan) << std::endl;
                }
            } catch (const TracedException& e) {
                std::cerr << "VelODB Error: " << e.message() << std::endl;
                if (program["--verbose"] == true) {
                    std::cerr << "\nFull error with stack trace:" << std::endl;
                    std::cerr << e.what() << std::endl;
                }
                return -1;
            } catch (const std::exception& e) {
                std::cerr << "Error planning query: " << e.what() << std::endl;
                return -1;
            }
        } else {
            std::cerr << "Only SELECT queries are supported" << std::endl;
            return -1;
        }
    } else {
        std::cerr << "Please use --mock-catalog or --tpch-catalog flag for query planning" << std::endl;
        return -1;
    }

    return 0;
}
