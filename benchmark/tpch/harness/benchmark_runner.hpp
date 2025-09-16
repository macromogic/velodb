#pragma once

#include "velodb.hpp"

#include "../../infrastructure/performance_monitor.hpp"
#include "../data/loaders/tpch_data_loader.hpp"
#include "common/copy_traits.hpp"
#include "common/result.hpp"
#include "execution/query_result.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace velodb::benchmark::tpch {

class TPCHBenchmarkRunner : public NonCopyable {
public:
    struct BenchmarkConfig {
        std::vector<double> scale_factors = { 0.01, 0.1, 1.0 };
        std::vector<int> query_numbers = { 1, 6, 12 }; // Start with simple queries
        int iterations = 3;
        bool measure_power_test = true;
        bool measure_throughput_test = false;
        bool validate_results = true;
        bool verbose = true;
        std::string data_directory = "./benchmark/data";
        std::string results_directory = "./benchmark/results";
    };

    struct QueryResult {
        int query_number;
        double scale_factor;
        int iteration;
        PerformanceMonitor::QueryMetrics metrics;
        bool success;
        std::string error_message;
        size_t result_row_count = 0;
    };

    struct BenchmarkResults {
        std::vector<QueryResult> query_results;
        std::chrono::duration<double> total_benchmark_time { 0 };
        std::chrono::duration<double> data_load_time { 0 };
        PerformanceMonitor::SystemInfo system_info;
        BenchmarkConfig config;
        std::string benchmark_id;
        bool success = true;
        std::string error_message;
    };

    TPCHBenchmarkRunner();
    ~TPCHBenchmarkRunner();

    // Main benchmark execution methods
    Result<BenchmarkResults> runPowerTest(const BenchmarkConfig& config);
    Result<BenchmarkResults> runFullBenchmark(const BenchmarkConfig& config);

    // Individual query execution
    Result<QueryResult> runSingleQuery(int query_number, double scale_factor, int iteration = 1);

    // Data management
    Result<void> loadBenchmarkData(const BenchmarkConfig& config);
    Result<void> validateBenchmarkData(double scale_factor);

    // Results management
    Result<void> exportResults(const BenchmarkResults& results, const std::string& format = "json");
    Result<void> exportSummaryReport(const BenchmarkResults& results);

private:
    Database database_;
    PerformanceMonitor performance_monitor_;
    std::unordered_map<double, bool> loaded_scale_factors_;

    // Query execution helpers
    std::string loadQueryTemplate(int query_number);
    std::string substituteQueryParameters(const std::string& query_template, int query_number);
    Result<velodb::QueryResult> executeQuery(const std::string& sql, int query_number);

    // Data validation helpers
    bool validateQueryResult(int query_number, const velodb::QueryResult& result);

    // Utility methods
    std::string generateBenchmarkId() const;
    std::string formatBenchmarkSummary(const BenchmarkResults& results) const;
    std::string getQueryName(int query_number) const;
};

// Query parameter generator for TPC-H queries
class QueryParameterGenerator {
public:
    struct Parameters {
        std::unordered_map<std::string, std::string> substitutions;
        int stream_id;
        int query_number;
    };

    static std::vector<Parameters> generateParameters(int query_number, int stream_count = 1);
    static std::string substituteParameters(const std::string& template_query, const Parameters& params);

private:
    static std::unordered_map<std::string, std::string> getDefaultParameters(int query_number);
};

} // namespace velodb::benchmark::tpch
