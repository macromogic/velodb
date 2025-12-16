#include "performance_monitor.hpp"

#include "common/fmt.hpp"

#include <fmt/chrono.h>
#include <fmt/core.h>

#include <fstream>
#include <iomanip>
#include <sstream>

#ifdef __linux__
#include <fstream>

#include <sys/resource.h>
#include <unistd.h>
#endif

#ifdef CUDA_ENABLED
#include <cuda_runtime.h>
#endif

namespace velodb::benchmark {

PerformanceMonitor::PerformanceMonitor()
    : baseline_memory_(getMemorySnapshot())
{
}

PerformanceMonitor::~PerformanceMonitor() = default;

void PerformanceMonitor::startQuery(const std::string& query_name)
{
    current_query_ = query_name;
    query_start_time_ = std::chrono::steady_clock::now();
    baseline_memory_ = getMemorySnapshot();
    peak_memory_usage_ = baseline_memory_.resident_set_size;

    // Initialize metrics for this query
    metrics_[query_name] = QueryMetrics {};
}

PerformanceMonitor::QueryMetrics PerformanceMonitor::finishQuery()
{
    if (current_query_.empty()) {
        return QueryMetrics {};
    }

    auto end_time = std::chrono::steady_clock::now();
    auto& metric = metrics_[current_query_];

    metric.execution_time = end_time - query_start_time_;
    metric.memory_usage_peak = peak_memory_usage_ - baseline_memory_.resident_set_size;
    metric.gpu_memory_usage = getGpuMemoryUsage();
    metric.success = true;

    std::string completed_query = current_query_;
    current_query_.clear();

    return metric;
}

void PerformanceMonitor::recordPlanningTime(std::chrono::duration<double> time)
{
    if (!current_query_.empty()) {
        metrics_[current_query_].planning_time = time;
    }
}

void PerformanceMonitor::recordMemoryUsage(size_t bytes)
{
    peak_memory_usage_ = std::max(peak_memory_usage_, bytes);
}

void PerformanceMonitor::recordRowsProcessed(size_t rows)
{
    if (!current_query_.empty()) {
        metrics_[current_query_].rows_processed = rows;
    }
}

void PerformanceMonitor::recordLateMaterialization(bool used)
{
    if (!current_query_.empty()) {
        metrics_[current_query_].used_late_materialization = used;
    }
}

PerformanceMonitor::SystemInfo PerformanceMonitor::getSystemInfo() const
{
    SystemInfo info;
    auto now = std::chrono::system_clock::now();
    info.timestamp = fmt::format("{:%Y-%m-%d %H:%M:%S}", now);

#ifdef __linux__
    // Get CPU info
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.find("model name") != std::string::npos) {
            info.cpu_info = line.substr(line.find(":") + 2);
            break;
        }
    }

    // Get memory info
    std::ifstream meminfo("/proc/meminfo");
    while (std::getline(meminfo, line)) {
        if (line.find("MemTotal:") == 0) {
            std::istringstream iss(line);
            std::string label;
            size_t value;
            iss >> label >> value;
            info.total_memory = value * 1024; // Convert KB to bytes
        } else if (line.find("MemAvailable:") == 0) {
            std::istringstream iss(line);
            std::string label;
            size_t value;
            iss >> label >> value;
            info.available_memory = value * 1024; // Convert KB to bytes
            break;
        }
    }
#endif

#ifdef CUDA_ENABLED
    // Get GPU info
    int device_count = 0;
    cudaGetDeviceCount(&device_count);
    if (device_count > 0) {
        cudaDeviceProp prop;
        cudaGetDeviceProperties(&prop, 0);
        info.gpu_info = fmt::format("{} (Compute Capability {}.{})", prop.name, prop.major, prop.minor);
    }
#endif

    return info;
}

Result<void> PerformanceMonitor::exportResults(const std::string& filename, const std::string& format)
{
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return Result<void>::failure(fmt::format("Failed to open file: {}", filename));
        }

        if (format == "json") {
            file << formatMetricsAsJson();
        } else if (format == "csv") {
            file << formatMetricsAsCsv();
        } else {
            return Result<void>::failure(fmt::format("Unsupported format: {}", format));
        }

        file.close();
        return Result<void>::success();
    } catch (const std::exception& e) {
        return Result<void>::failure(fmt::format("Export failed: {}", e.what()));
    }
}

Result<void> PerformanceMonitor::exportSummary(const std::string& filename)
{
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return Result<void>::failure(fmt::format("Failed to open file: {}", filename));
        }

        file << formatSummary();
        file.close();
        return Result<void>::success();
    } catch (const std::exception& e) {
        return Result<void>::failure(fmt::format("Summary export failed: {}", e.what()));
    }
}

PerformanceMonitor::MemorySnapshot PerformanceMonitor::getMemorySnapshot() const
{
    MemorySnapshot snapshot {};
    snapshot.timestamp = std::chrono::steady_clock::now();

#ifdef __linux__
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.find("VmRSS:") == 0) {
            std::istringstream iss(line);
            std::string label;
            size_t value;
            iss >> label >> value;
            snapshot.resident_set_size = value * 1024; // Convert KB to bytes
        } else if (line.find("VmSize:") == 0) {
            std::istringstream iss(line);
            std::string label;
            size_t value;
            iss >> label >> value;
            snapshot.virtual_memory_size = value * 1024; // Convert KB to bytes
        }
    }
#endif

    return snapshot;
}

size_t PerformanceMonitor::getGpuMemoryUsage() const
{
#ifdef CUDA_ENABLED
    size_t free_bytes = 0;
    size_t total_bytes = 0;
    cudaMemGetInfo(&free_bytes, &total_bytes);
    return total_bytes - free_bytes;
#else
    return 0;
#endif
}

std::string PerformanceMonitor::formatMetricsAsJson() const
{
    auto system_info = getSystemInfo();
    std::ostringstream json;

    json << "{\n";
    json << "  \"system_info\": {\n";
    json << "    \"timestamp\": \"" << system_info.timestamp << "\",\n";
    json << "    \"cpu\": \"" << system_info.cpu_info << "\",\n";
    json << "    \"gpu\": \"" << system_info.gpu_info << "\",\n";
    json << "    \"total_memory\": " << system_info.total_memory << ",\n";
    json << "    \"available_memory\": " << system_info.available_memory << "\n";
    json << "  },\n";
    json << "  \"queries\": {\n";

    bool first = true;
    for (const auto& [query_name, metric] : metrics_) {
        if (!first)
            json << ",\n";
        first = false;

        json << "    \"" << query_name << "\": {\n";
        json << "      \"execution_time\": " << metric.execution_time.count() << ",\n";
        json << "      \"planning_time\": " << metric.planning_time.count() << ",\n";
        json << "      \"memory_peak\": " << metric.memory_usage_peak << ",\n";
        json << "      \"gpu_memory\": " << metric.gpu_memory_usage << ",\n";
        json << "      \"rows_processed\": " << metric.rows_processed << ",\n";
        json << "      \"bytes_processed\": " << metric.bytes_processed << ",\n";
        json << "      \"late_materialization\": " << (metric.used_late_materialization ? "true" : "false") << ",\n";
        json << "      \"success\": " << (metric.success ? "true" : "false");
        if (!metric.error_message.empty()) {
            json << ",\n      \"error\": \"" << metric.error_message << "\"";
        }
        json << "\n    }";
    }

    json << "\n  }\n";
    json << "}\n";

    return json.str();
}

std::string PerformanceMonitor::formatMetricsAsCsv() const
{
    std::ostringstream csv;

    // Header
    csv << "Query,ExecutionTime,PlanningTime,MemoryPeak,GpuMemory,RowsProcessed,BytesProcessed,LateMaterialization,"
           "Success,Error\n";

    // Data rows
    for (const auto& [query_name, metric] : metrics_) {
        csv << query_name << "," << metric.execution_time.count() << "," << metric.planning_time.count() << ","
            << metric.memory_usage_peak << "," << metric.gpu_memory_usage << "," << metric.rows_processed << ","
            << metric.bytes_processed << "," << (metric.used_late_materialization ? "true" : "false") << ","
            << (metric.success ? "true" : "false") << ","
            << "\"" << metric.error_message << "\"\n";
    }

    return csv.str();
}

std::string PerformanceMonitor::formatSummary() const
{
    std::ostringstream summary;
    auto system_info = getSystemInfo();

    summary << "VelODB TPC-H Benchmark Summary\n";
    summary << "============================\n\n";
    summary << "Timestamp: " << system_info.timestamp << "\n";
    summary << "CPU: " << system_info.cpu_info << "\n";
    summary << "GPU: " << system_info.gpu_info << "\n";
    summary << "Total Memory: " << (system_info.total_memory / (1024 * 1024)) << " MB\n\n";

    summary << "Query Results:\n";
    summary << "--------------\n";

    double total_time = 0;
    size_t successful_queries = 0;
    size_t late_mat_queries = 0;

    for (const auto& [query_name, metric] : metrics_) {
        if (metric.success) {
            successful_queries++;
            total_time += metric.execution_time.count();
            if (metric.used_late_materialization) {
                late_mat_queries++;
            }

            summary << fmt::format("{:>8}: {:>8.3f}s  {:>8.3f}s  {:>8} MB  {:>8} rows",
                                   query_name,
                                   metric.execution_time.count(),
                                   metric.planning_time.count(),
                                   metric.memory_usage_peak / (1024 * 1024),
                                   metric.rows_processed);

            if (metric.used_late_materialization) {
                summary << "  [LM]";
            }
            summary << "\n";
        } else {
            summary << fmt::format("{:>8}: FAILED - {}\n", query_name, metric.error_message);
        }
    }

    summary << "\nSummary Statistics:\n";
    summary << "-------------------\n";
    summary << fmt::format("Total Queries: {}\n", metrics_.size());
    summary << fmt::format("Successful: {}\n", successful_queries);
    summary << fmt::format("Total Time: {:.3f}s\n", total_time);
    summary << fmt::format("Average Time: {:.3f}s\n", successful_queries > 0 ? total_time / successful_queries : 0.0);
    summary << fmt::format("Late Materialization Usage: {}/{}\n", late_mat_queries, successful_queries);

    return summary.str();
}

} // namespace velodb::benchmark
