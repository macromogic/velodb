#pragma once

#include "common/copy_traits.hpp"
#include "common/result.hpp"
#include "execution/query_result.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>

namespace velodb::benchmark {

class PerformanceMonitor : public NonCopyable {
public:
    struct QueryMetrics {
        std::chrono::duration<double> execution_time { 0 };
        std::chrono::duration<double> planning_time { 0 };
        size_t memory_usage_peak = 0;
        size_t gpu_memory_usage = 0;
        size_t rows_processed = 0;
        size_t bytes_processed = 0;
        std::string error_message;
        bool success = false;
    };

    struct SystemInfo {
        std::string cpu_info;
        std::string gpu_info;
        size_t total_memory;
        size_t available_memory;
        std::string timestamp;
    };

    PerformanceMonitor();
    ~PerformanceMonitor() = default;

    // Query monitoring
    void startQuery(const std::string& query_name);
    QueryMetrics finishQuery(const velodb::QueryStatistics& stats);
    void recordMemoryUsage(size_t bytes);
    void recordRowsProcessed(size_t rows);

    // System information
    SystemInfo getSystemInfo() const;

    // Results export
    Result<void> exportResults(const std::string& filename, const std::string& format = "json");
    Result<void> exportSummary(const std::string& filename);

    // Get all recorded metrics
    const std::unordered_map<std::string, QueryMetrics>& getAllMetrics() const { return metrics_; }

private:
    struct MemorySnapshot {
        size_t resident_set_size;
        size_t virtual_memory_size;
        std::chrono::steady_clock::time_point timestamp;
    };

    std::string current_query_;
    std::unordered_map<std::string, QueryMetrics> metrics_;

    // Memory monitoring
    MemorySnapshot getMemorySnapshot() const;
    MemorySnapshot baseline_memory_;
    size_t peak_memory_usage_ = 0;

    // GPU monitoring (if available)
    size_t getGpuMemoryUsage() const;

    // Helper methods
    std::string formatMetricsAsJson() const;
    std::string formatMetricsAsCsv() const;
    std::string formatSummary() const;
};

} // namespace velodb::benchmark
