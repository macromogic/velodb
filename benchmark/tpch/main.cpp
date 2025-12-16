#include "common/fmt.hpp"
#include "harness/benchmark_runner.hpp"

#include <argparse/argparse.hpp>
#include <fmt/ranges.h>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace velodb::benchmark::tpch;

std::vector<double> parseScaleFactors(const std::string& scale_factors_str)
{
    std::vector<double> scale_factors;
    std::stringstream ss(scale_factors_str);
    std::string item;

    while (std::getline(ss, item, ',')) {
        try {
            double sf = std::stod(item);
            scale_factors.push_back(sf);
        } catch (const std::exception& e) {
            throw std::invalid_argument(fmt::format("Invalid scale factor: {}", item));
        }
    }

    return scale_factors;
}

std::vector<int> parseQueryNumbers(const std::string& queries_str)
{
    std::vector<int> queries;
    std::stringstream ss(queries_str);
    std::string item;

    while (std::getline(ss, item, ',')) {
        try {
            int query = std::stoi(item);
            if (query < 1 || query > 22) {
                throw std::invalid_argument(fmt::format("Query number must be between 1-22, got: {}", query));
            }
            queries.push_back(query);
        } catch (const std::exception& e) {
            throw std::invalid_argument(fmt::format("Invalid query number: {}", item));
        }
    }

    return queries;
}

int main(int argc, char* argv[])
{
    argparse::ArgumentParser program("tpch_benchmark", "1.0");

    program.add_description("VelODB TPC-H Benchmark Suite");

    // Scale factor configuration
    program.add_argument("--scale-factor", "-s")
        .help("Scale factors to test (comma-separated)")
        .default_value(std::string("0.01"))
        .metavar("SF");

    // Query selection
    program.add_argument("--queries", "-q")
        .help("Query numbers to run (comma-separated)")
        .default_value(std::string("1,6"))
        .metavar("QUERIES");

    // Iteration count
    program.add_argument("--iterations", "-i")
        .help("Number of iterations per query")
        .default_value(1)
        .scan<'i', int>()
        .metavar("N");

    // Data directory
    program.add_argument("--data-dir", "-d")
        .help("Directory containing TPC-H data files")
        .default_value(std::string("./benchmark/data"))
        .metavar("DIR");

    // Results directory
    program.add_argument("--results-dir", "-r")
        .help("Directory to store benchmark results")
        .default_value(std::string("./benchmark/results"))
        .metavar("DIR");

    // Test type
    program.add_argument("--test-type", "-t")
        .help("Type of test to run")
        .default_value(std::string("power"))
        .choices("power", "throughput", "full")
        .metavar("TYPE");

    // Output options
    program.add_argument("--verbose", "-v").help("Enable verbose output").flag();

    program.add_argument("--export-format")
        .help("Export format for detailed results")
        .default_value(std::string("json"))
        .choices("json", "csv")
        .metavar("FORMAT");

    program.add_argument("--validate").help("Validate query results").default_value(true).implicit_value(true);

    // Quick benchmark presets
    program.add_argument("--quick").help("Run quick benchmark (SF=0.01, Q1,Q6, 1 iteration)").flag();

    program.add_argument("--standard").help("Run standard benchmark (SF=0.1, Q1,Q3,Q6,Q12, 3 iterations)").flag();

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        std::cerr << "Error: " << err.what() << std::endl;
        std::cerr << program;
        return 1;
    }

    try {
        // Create benchmark configuration
        TPCHBenchmarkRunner::BenchmarkConfig config;

        // Apply presets first
        if (program.get<bool>("--quick")) {
            config.scale_factors = { 0.01 };
            config.query_numbers = { 1, 6 };
            config.iterations = 1;
        } else if (program.get<bool>("--standard")) {
            config.scale_factors = { 0.1 };
            config.query_numbers = { 1, 3, 6, 12 };
            config.iterations = 3;
        } else {
            // Parse individual arguments
            config.scale_factors = parseScaleFactors(program.get<std::string>("--scale-factor"));
            config.query_numbers = parseQueryNumbers(program.get<std::string>("--queries"));
            config.iterations = program.get<int>("--iterations");
        }

        config.data_directory = program.get<std::string>("--data-dir");
        config.results_directory = program.get<std::string>("--results-dir");
        config.validate_results = program.get<bool>("--validate");
        config.verbose = program.get<bool>("--verbose");

        std::string test_type = program.get<std::string>("--test-type");
        config.measure_power_test = (test_type == "power" || test_type == "full");
        config.measure_throughput_test = (test_type == "throughput" || test_type == "full");

        if (config.verbose) {
            fmt::println("VelODB TPC-H Benchmark");
            fmt::println("======================");
            fmt::println("Scale Factors: {}", fmt::join(config.scale_factors, ", "));
            fmt::println("Queries: {}", fmt::join(config.query_numbers, ", "));
            fmt::println("Iterations: {}", config.iterations);
            fmt::println("Data Directory: {}", config.data_directory);
            fmt::println("Results Directory: {}\n", config.results_directory);
        }

        // Create and run benchmark
        TPCHBenchmarkRunner runner;

        auto result = runner.runFullBenchmark(config);
        if (!result) {
            fmt::println(stderr, "Benchmark failed: {}", result.error());
            return 1;
        }

        const auto& benchmark_results = result.value();

        // Export detailed results
        std::string export_format = program.get<std::string>("--export-format");
        auto export_result = runner.exportResults(benchmark_results, export_format);
        if (!export_result) {
            fmt::println(stderr, "Warning: Failed to export detailed results: {}", export_result.error());
        }

        // Export summary report
        auto summary_result = runner.exportSummaryReport(benchmark_results);
        if (!summary_result) {
            fmt::println(stderr, "Warning: Failed to export summary report: {}", summary_result.error());
        } else if (config.verbose) {
            fmt::println("Results exported to: {}", config.results_directory);
        }

        // Print summary to console
        if (config.verbose) {
            size_t successful = 0;
            double total_time = 0;

            for (const auto& qr : benchmark_results.query_results) {
                if (qr.success) {
                    successful++;
                    total_time += qr.metrics.execution_time.count();
                }
            }

            fmt::println("\nBenchmark Summary:");
            fmt::println("------------------");
            fmt::println("Total Queries: {}", benchmark_results.query_results.size());
            fmt::println("Successful: {} ({:.1f}%)",
                         successful,
                         100.0 * successful / benchmark_results.query_results.size());
            if (successful > 0) {
                fmt::println("Average Query Time: {:.3f}s", total_time / successful);
            }
            fmt::println("Total Benchmark Time: {:.3f}s", benchmark_results.total_benchmark_time.count());
        }

        return benchmark_results.success ? 0 : 1;

    } catch (const std::exception& e) {
        fmt::println(stderr, "Error: {}", e.what());
        return 1;
    }
}
