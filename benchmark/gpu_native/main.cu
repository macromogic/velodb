#include "common/types.hpp"

#include "common/data_loader.cuh"
#include "common/hash_table.cuh"
#include "common/launcher.cuh"
#include "queries/q01.cuh"
#include "queries/q03.cuh"
#include "queries/q04.cuh"
#include "queries/q05.cuh"
#include "queries/q06.cuh"
#include "queries/q10.cuh"
#include "queries/q12.cuh"
#include "queries/q14.cuh"
#include "queries/q15.cuh"
#include "queries/q17.cuh"
#include "queries/q18.cuh"
#include "queries/q19.cuh"

#include <argparse/argparse.hpp>
#include <fmt/core.h>

#include <chrono>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

using namespace gpu_native;

// ============================================================================
// Benchmark Runner
// ============================================================================

struct BenchmarkResult {
    std::string query_name;
    double kernel_time_ms;
};

template <typename RunFunc>
BenchmarkResult run_benchmark(const std::string& name, RunFunc&& run_func, int warmup_runs = 2, int benchmark_runs = 5)
{
    BenchmarkResult result;
    result.query_name = name;

    cudaStream_t stream;
    GPU_CHECK(cudaStreamCreate(&stream));

    // Warmup runs
    for (int i = 0; i < warmup_runs; i++) {
        run_func(stream);
        GPU_CHECK(cudaStreamSynchronize(stream));
    }

    // Benchmark runs
    std::vector<double> times;
    GpuTimer timer(stream);

    for (int i = 0; i < benchmark_runs; i++) {
        timer.start();
        run_func(stream); // Ignore result count
        timer.stop();

        times.push_back(timer.elapsed_ms());
    }

    // Calculate average (excluding min and max if enough runs)
    if (times.size() >= 5) {
        std::sort(times.begin(), times.end());
        double sum = 0;
        for (size_t i = 1; i < times.size() - 1; i++) {
            sum += times[i];
        }
        result.kernel_time_ms = sum / (times.size() - 2);
    } else {
        double sum = 0;
        for (auto t : times)
            sum += t;
        result.kernel_time_ms = sum / times.size();
    }

    GPU_CHECK(cudaStreamDestroy(stream));

    return result;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[])
{
    argparse::ArgumentParser program("gpu_native_benchmark");

    program.add_argument("-d", "--data-dir").default_value(std::string("../data")).help("Path to TPC-H data directory");

    program.add_argument("-o", "--output").default_value(std::string("")).help("Output CSV file path (optional)");

    program.add_argument("-q", "--query")
        .default_value(std::string("all"))
        .help("Query to run (q01, q03, q04, q05, q06, q10, q12, q14, q15, q17, q18, q19, or all)");

    program.add_argument("-w", "--warmup").default_value(2).scan<'i', int>().help("Number of warmup runs");

    program.add_argument("-r", "--runs").default_value(5).scan<'i', int>().help("Number of benchmark runs");

    try {
        program.parse_args(argc, argv);
    } catch (const std::runtime_error& err) {
        fmt::println(stderr, "{}", err.what());
        fmt::println(stderr, "{}", program.help().str());
        return 1;
    }

    std::string data_dir = program.get<std::string>("--data-dir");
    std::string output_file = program.get<std::string>("--output");
    std::string query = program.get<std::string>("--query");
    int warmup_runs = program.get<int>("--warmup");
    int benchmark_runs = program.get<int>("--runs");

    fmt::println("GPU Native TPC-H Benchmark");
    fmt::println("==========================");
    fmt::println("Data directory: {}", data_dir);
    fmt::println("Output file: {}", output_file.empty() ? "(stdout)" : output_file);
    fmt::println("Query: {}", query);
    fmt::println("Warmup runs: {}", warmup_runs);
    fmt::println("Benchmark runs: {}", benchmark_runs);
    fmt::println("");

    // Load data
    fmt::println("Loading TPC-H tables...");
    auto start = std::chrono::high_resolution_clock::now();
    TPCHTables tables = load_tpch_tables(data_dir);
    auto end = std::chrono::high_resolution_clock::now();
    double load_time = std::chrono::duration<double, std::milli>(end - start).count();
    fmt::println("Data loaded in {:.2f} ms\n", load_time);

    // Allocate result buffers (sized for SF=1)
    size_t max_result_size = tables.lineitem.num_rows; // Conservative estimate

    std::vector<BenchmarkResult> results;

    // Run queries
    auto run_query = [&](const std::string& q) {
        if (q == "q01" || q == "all") {
            Q01Result r = allocate_q01_result(max_result_size);
            auto res = run_benchmark(
                "Q01",
                [&](cudaStream_t s) { return run_q01(tables.lineitem, r, s); },
                warmup_runs,
                benchmark_runs);
            results.push_back(res);
            free_q01_result(r);
        }

        if (q == "q03" || q == "all") {
            Q03Result r = allocate_q03_result(max_result_size);
            auto res = run_benchmark(
                "Q03",
                [&](cudaStream_t s) { return run_q03(tables.customer, tables.orders, tables.lineitem, r, s); },
                warmup_runs,
                benchmark_runs);
            results.push_back(res);
            free_q03_result(r);
        }

        if (q == "q04" || q == "all") {
            Q04Result r = allocate_q04_result(tables.orders.num_rows);
            auto res = run_benchmark(
                "Q04",
                [&](cudaStream_t s) { return run_q04(tables.orders, tables.lineitem, r, s); },
                warmup_runs,
                benchmark_runs);
            results.push_back(res);
            free_q04_result(r);
        }

        if (q == "q05" || q == "all") {
            Q05Result r = allocate_q05_result(max_result_size);
            auto res = run_benchmark(
                "Q05",
                [&](cudaStream_t s) {
                    return run_q05(tables.region,
                                   tables.nation,
                                   tables.supplier,
                                   tables.customer,
                                   tables.orders,
                                   tables.lineitem,
                                   r,
                                   s);
                },
                warmup_runs,
                benchmark_runs);
            results.push_back(res);
            free_q05_result(r);
        }

        if (q == "q06" || q == "all") {
            Q06Result r = allocate_q06_result(max_result_size);
            auto res = run_benchmark(
                "Q06",
                [&](cudaStream_t s) { return run_q06(tables.lineitem, r, s); },
                warmup_runs,
                benchmark_runs);
            results.push_back(res);
            free_q06_result(r);
        }

        if (q == "q10" || q == "all") {
            Q10Result r = allocate_q10_result(max_result_size);
            auto res = run_benchmark(
                "Q10",
                [&](cudaStream_t s) {
                    return run_q10(tables.nation, tables.customer, tables.orders, tables.lineitem, r, s);
                },
                warmup_runs,
                benchmark_runs);
            results.push_back(res);
            free_q10_result(r);
        }

        if (q == "q12" || q == "all") {
            Q12Result r = allocate_q12_result(max_result_size);
            auto res = run_benchmark(
                "Q12",
                [&](cudaStream_t s) { return run_q12(tables.orders, tables.lineitem, r, s); },
                warmup_runs,
                benchmark_runs);
            results.push_back(res);
            free_q12_result(r);
        }

        if (q == "q14" || q == "all") {
            Q14Result r = allocate_q14_result(max_result_size);
            auto res = run_benchmark(
                "Q14",
                [&](cudaStream_t s) { return run_q14(tables.part, tables.lineitem, r, s); },
                warmup_runs,
                benchmark_runs);
            results.push_back(res);
            free_q14_result(r);
        }

        if (q == "q15" || q == "all") {
            Q15Result r = allocate_q15_result(max_result_size);
            auto res = run_benchmark(
                "Q15",
                [&](cudaStream_t s) { return run_q15(tables.supplier, tables.lineitem, r, s); },
                warmup_runs,
                benchmark_runs);
            results.push_back(res);
            free_q15_result(r);
        }

        if (q == "q17" || q == "all") {
            Q17Result r = allocate_q17_result(max_result_size);
            auto res = run_benchmark(
                "Q17",
                [&](cudaStream_t s) { return run_q17(tables.part, tables.lineitem, r, s); },
                warmup_runs,
                benchmark_runs);
            results.push_back(res);
            free_q17_result(r);
        }

        if (q == "q18" || q == "all") {
            Q18Result r = allocate_q18_result(max_result_size);
            auto res = run_benchmark(
                "Q18",
                [&](cudaStream_t s) { return run_q18(tables.customer, tables.orders, tables.lineitem, r, s); },
                warmup_runs,
                benchmark_runs);
            results.push_back(res);
            free_q18_result(r);
        }

        if (q == "q19" || q == "all") {
            Q19Result r = allocate_q19_result(max_result_size);
            auto res = run_benchmark(
                "Q19",
                [&](cudaStream_t s) { return run_q19(tables.part, tables.lineitem, r, s); },
                warmup_runs,
                benchmark_runs);
            results.push_back(res);
            free_q19_result(r);
        }
    };

    run_query(query);

    // Calculate total time
    double total_time = 0;
    for (const auto& r : results) {
        total_time += r.kernel_time_ms;
    }

    // Output to CSV if specified
    if (!output_file.empty()) {
        std::ofstream ofs(output_file);
        ofs << "query,time_ms\n";
        for (const auto& r : results) {
            ofs << r.query_name << "," << r.kernel_time_ms << "\n";
        }
        ofs << "TOTAL," << total_time << "\n";
        ofs.close();
        fmt::println("Results written to: {}", output_file);
    }

    // Print results to stdout
    fmt::println("\n{:=^60}", " Results ");
    fmt::println("{:<10} {:>15}", "Query", "Time (ms)");
    fmt::println("{:-^60}", "");

    for (const auto& r : results) {
        fmt::println("{:<10} {:>15.3f}", r.query_name, r.kernel_time_ms);
    }

    fmt::println("{:-^60}", "");
    fmt::println("{:<10} {:>15.3f}", "TOTAL", total_time);
    fmt::println("");

    // Cleanup
    free_tpch_tables(tables);

    return 0;
}
