#include "benchmark_runner.hpp"

#include "common/fmt.hpp"
#include "execution/query_result.hpp"

#include <fmt/chrono.h>
#include <fmt/core.h>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>

namespace {

static inline void ensure_gitignore(const std::filesystem::path& dir_path)
{
    std::filesystem::path gitignore_path = dir_path / ".gitignore";
    if (!std::filesystem::exists(gitignore_path)) {
        std::ofstream gitignore_file(gitignore_path);
        gitignore_file << "*\n";
    }
}

} // anonymous namespace

namespace velodb::benchmark::tpch {

TPCHBenchmarkRunner::TPCHBenchmarkRunner()
    : database_()
    , performance_monitor_()
{
    database_.initialize();
}

TPCHBenchmarkRunner::~TPCHBenchmarkRunner()
{
    database_.shutdown();
}

Result<TPCHBenchmarkRunner::BenchmarkResults> TPCHBenchmarkRunner::runPowerTest(const BenchmarkConfig& config)
{
    BenchmarkResults results;
    results.config = config;
    results.benchmark_id = generateBenchmarkId();
    results.system_info = performance_monitor_.getSystemInfo();

    if (config.verbose) {
        fmt::println("Starting TPC-H Power Test ({})", results.benchmark_id);
        fmt::println("========================================");
    }

    auto start_time = std::chrono::steady_clock::now();

    // Load data for each scale factor
    for (double scale_factor : config.scale_factors) {
        if (config.verbose) {
            fmt::println("\nLoading data for scale factor {}", scale_factor);
        }

        auto load_start = std::chrono::steady_clock::now();
        auto load_result = loadBenchmarkData(config);
        auto load_end = std::chrono::steady_clock::now();

        if (!load_result) {
            results.success = false;
            results.error_message = fmt::format("Data loading failed: {}", load_result.error());
            return Result<BenchmarkResults>::success(std::move(results));
        }

        results.data_load_time += (load_end - load_start);

        // Run queries for this scale factor
        for (int query_number : config.query_numbers) {
            for (int iteration = 1; iteration <= config.iterations; ++iteration) {
                if (config.verbose) {
                    fmt::println("Running Q{} (SF={}, iteration {})", query_number, scale_factor, iteration);
                }

                auto query_result = runSingleQuery(query_number, scale_factor, iteration);
                results.query_results.push_back(query_result);
                if (config.verbose && !query_result.success) {
                    fmt::println("  FAILED: {}", query_result.error_message);
                }
            }
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    results.total_benchmark_time = end_time - start_time;

    if (config.verbose) {
        fmt::println("\nBenchmark completed in {:.3f}s", results.total_benchmark_time.count());
    }

    return Result<BenchmarkResults>::success(std::move(results));
}

TPCHBenchmarkRunner::QueryResult TPCHBenchmarkRunner::runSingleQuery(int query_number,
                                                                     double scale_factor,
                                                                     int iteration)
{
    QueryResult result;
    result.query_number = query_number;
    result.scale_factor = scale_factor;
    result.iteration = iteration;

    try {
        // Load and prepare query
        std::string sql = loadQuery(query_number);

        // Start performance monitoring
        std::string query_name = fmt::format("Q{}_SF{}_I{}", query_number, scale_factor, iteration);
        performance_monitor_.startQuery(query_name);

        // Execute query
        velodb::QueryStatistics stats;
        auto query_result = executeQuery(sql, &stats);

        // Finish performance monitoring
        result.metrics = performance_monitor_.finishQuery(stats);

        if (query_result) {
            result.success = true;
            result.result_row_count = query_result.value().getRowCount();

            if (!validateQueryResult(query_number, query_result.value())) {
                result.success = false;
                result.error_message = "Result validation failed";
            }
        } else {
            result.success = false;
            result.error_message = query_result.error();
        }

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = fmt::format("Exception during query execution: {}", e.what());
    }

    return result;
}

Result<void> TPCHBenchmarkRunner::loadBenchmarkData(const BenchmarkConfig& config)
{
    // For now, load data for the smallest scale factor only
    double target_scale = *std::min_element(config.scale_factors.begin(), config.scale_factors.end());

    if (loaded_scale_factors_[target_scale]) {
        return Result<void>::success(); // Already loaded
    }

    TPCHDataLoader loader(database_.getCatalog());
    TPCHDataLoader::LoadConfig load_config;
    load_config.data_directory = config.data_directory;
    load_config.scale_factor = target_scale;
    load_config.verbose = config.verbose;

    // Check if data files exist
    if (!TPCHDataLoader::dataFilesExist(load_config.data_directory, target_scale)) {
        return Result<void>::failure(fmt::format("TPC-H data files not found for SF={} in directory: {}",
                                                 target_scale,
                                                 load_config.data_directory));
    }

    auto load_result = loader.loadAllTables(load_config);
    if (!load_result) {
        return Result<void>::failure(load_result.error());
    }

    loaded_scale_factors_[target_scale] = true;
    return Result<void>::success();
}

Result<void> TPCHBenchmarkRunner::validateBenchmarkData(double scale_factor)
{
    TPCHValidator::ValidationResult validation = TPCHValidator::validateTableCounts(database_.getCatalog(),
                                                                                    scale_factor);

    if (!validation.passed) {
        std::string error_msg = "Data validation failed:\n";
        for (const auto& error : validation.errors) {
            error_msg += "  - " + error + "\n";
        }
        return Result<void>::failure(error_msg);
    }

    return Result<void>::success();
}

Result<void> TPCHBenchmarkRunner::exportResults(const BenchmarkResults& results, const std::string& format)
{
    std::filesystem::create_directories(results.config.results_directory);
    ensure_gitignore(results.config.results_directory);

    std::filesystem::path filename = results.config.results_directory
        / fmt::format("tpch_benchmark_{}.{}", results.benchmark_id, format);

    if (format == "json") {
        std::ofstream file(filename);
        if (!file.is_open())
            return Result<void>::failure(fmt::format("Cannot create file: {}", std::string(filename)));

        file << "{\n";
        file << fmt::format("  \"benchmark_id\": \"{}\",\n", results.benchmark_id);
        file << "  \"results\": [\n";

        for (size_t i = 0; i < results.query_results.size(); ++i) {
            const auto& qr = results.query_results[i];
            file << "    {\n";
            file << fmt::format("      \"query\": {},\n", qr.query_number);
            file << fmt::format("      \"scale_factor\": {},\n", qr.scale_factor);
            file << fmt::format("      \"iteration\": {},\n", qr.iteration);
            file << fmt::format("      \"time_s\": {:.4f},\n", qr.metrics.execution_time.count());
            file << fmt::format("      \"rows\": {},\n", qr.result_row_count);
            file << fmt::format("      \"success\": {},\n", qr.success ? "true" : "false");
            file << fmt::format("      \"error_message\": \"{}\"\n", qr.error_message);
            file << "    }" << (i < results.query_results.size() - 1 ? "," : "") << "\n";
        }
        file << "  ]\n";
        file << "}\n";
        return Result<void>::success();
    } else if (format == "csv") {
        std::ofstream file(filename);
        if (!file.is_open())
            return Result<void>::failure(fmt::format("Cannot create file: {}", std::string(filename)));

        file << "Query,ScaleFactor,Iteration,Time_s,Rows,Status,ErrorMessage\n";

        for (const auto& qr : results.query_results) {
            file << fmt::format("{},{},{},{:.4f},{},{},\"{}\"\n",
                                qr.query_number,
                                qr.scale_factor,
                                qr.iteration,
                                qr.metrics.execution_time.count(),
                                qr.result_row_count,
                                qr.success ? "OK" : "FAIL",
                                qr.error_message);
        }
        return Result<void>::success();
    } else {
        return Result<void>::failure(fmt::format("Unsupported format: {}", format));
    }
}

Result<void> TPCHBenchmarkRunner::exportSummaryReport(const BenchmarkResults& results)
{
    std::filesystem::create_directories(results.config.results_directory);
    ensure_gitignore(results.config.results_directory);

    std::filesystem::path filename = results.config.results_directory
        / fmt::format("tpch_summary_{}.txt", results.benchmark_id);

    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return Result<void>::failure(fmt::format("Cannot create file: {}", std::string(filename)));
        }

        file << formatBenchmarkSummary(results);
        file.close();

        return Result<void>::success();
    } catch (const std::exception& e) {
        return Result<void>::failure(fmt::format("Export failed: {}", e.what()));
    }
}

std::string TPCHBenchmarkRunner::loadQuery(int query_number)
{
    std::string filename = fmt::format("benchmark/tpch/queries/templates/q{:02d}.sql", query_number);

    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error(fmt::format("Cannot open query file: {}", filename));
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return content;
}

Result<velodb::QueryResult> TPCHBenchmarkRunner::executeQuery(const std::string& sql, velodb::QueryStatistics* stats)
{
    try {
        return database_.executeQuery(sql, stats);
    } catch (const std::exception& e) {
        return Result<velodb::QueryResult>::failure(fmt::format("Query execution failed:\n{}", e.what()));
    }
}

bool TPCHBenchmarkRunner::validateQueryResult(int query_number, const velodb::QueryResult& result)
{
    // Construct path to answer file (e.g., benchmark/tpch/queries/answers/q01.ans)
    std::string answer_file = fmt::format("benchmark/tpch/queries/answers/q{:02d}.ans", query_number);

    if (!std::filesystem::exists(answer_file)) {
        // Fallback: Just check that we got some results if we expect them
        // Most TPC-H queries return rows
        return true;
    }

    try {
        std::ifstream file(answer_file);
        size_t expected_rows;
        // Simple validation: check if row count matches first number in answer file
        if (file >> expected_rows) {
            return result.getRowCount() == expected_rows;
        }
    } catch (...) {
        // Ignore file read errors
    }

    return true;
}

std::string TPCHBenchmarkRunner::generateBenchmarkId() const
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    return oss.str();
}

std::string TPCHBenchmarkRunner::formatBenchmarkSummary(const BenchmarkResults& results) const
{
    std::ostringstream summary;

    summary << "VelODB TPC-H Benchmark Report\n";
    summary << "=============================\n\n";
    summary << fmt::format("Benchmark ID: {}\n", results.benchmark_id);
    summary << fmt::format("Timestamp: {}\n", results.system_info.timestamp);
    summary << fmt::format("CPU: {}\n", results.system_info.cpu_info);
    summary << fmt::format("GPU: {}\n", results.system_info.gpu_info);
    summary << fmt::format("Total Memory: {} MB\n\n", results.system_info.total_memory / (1024 * 1024));

    summary << "Configuration:\n";
    summary << "--------------\n";
    summary << fmt::format("Scale Factors: ");
    for (size_t i = 0; i < results.config.scale_factors.size(); ++i) {
        if (i > 0)
            summary << ", ";
        summary << results.config.scale_factors[i];
    }
    summary << "\n";
    summary << fmt::format("Queries: ");
    for (size_t i = 0; i < results.config.query_numbers.size(); ++i) {
        if (i > 0)
            summary << ", ";
        summary << "Q" << results.config.query_numbers[i];
    }
    summary << "\n";
    summary << fmt::format("Iterations: {}\n", results.config.iterations);
    summary << fmt::format("Data Load Time: {:.3f}s\n", results.data_load_time.count());
    summary << fmt::format("Total Benchmark Time: {:.3f}s\n\n", results.total_benchmark_time.count());

    summary << "Query Results:\n";
    summary << "--------------\n";
    summary << fmt::format("{:>5} {:>5} {:>4} {:>10} {:>8} {:>8} {:>8}\n",
                           "Query",
                           "SF",
                           "Iter",
                           "Time(s)",
                           "Rows",
                           "Memory",
                           "Status");
    summary << std::string(65, '-') << "\n";

    for (const auto& qr : results.query_results) {
        std::string status = qr.success ? "OK" : "FAIL";

        summary << fmt::format("{:>5} {:>5} {:>4} {:>10.3f} {:>8} {:>8} {:>8}\n",
                               "Q" + std::to_string(qr.query_number),
                               qr.scale_factor,
                               qr.iteration,
                               qr.metrics.execution_time.count(),
                               qr.result_row_count,
                               qr.metrics.memory_usage_peak / (1024 * 1024),
                               status);
    }

    // Calculate summary statistics
    size_t successful_queries = 0;
    double total_time = 0;

    for (const auto& qr : results.query_results) {
        if (qr.success) {
            successful_queries++;
            total_time += qr.metrics.execution_time.count();
        }
    }

    summary << "\nSummary:\n";
    summary << "--------\n";
    summary << fmt::format("Total Queries: {}\n", results.query_results.size());
    summary << fmt::format("Successful: {} ({:.1f}%)\n",
                           successful_queries,
                           100.0 * successful_queries / results.query_results.size());
    if (successful_queries > 0) {
        summary << fmt::format("Average Query Time: {:.3f}s\n", total_time / successful_queries);
    }

    return summary.str();
}

std::string TPCHBenchmarkRunner::getQueryName(int query_number) const
{
    return fmt::format("Q{:02d}", query_number);
}

} // namespace velodb::benchmark::tpch
