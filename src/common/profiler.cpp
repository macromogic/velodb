#include "common/profiler.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <iomanip>
#include <iostream>

namespace velodb {

void Profiler::addTiming(const std::string& name, long long microseconds)
{
#if VELODB_ENABLE_PROFILING
    timings_[name].push_back(microseconds);
    total_time_[name] += microseconds;
    call_count_[name]++;
#endif // VELODB_ENABLE_PROFILING
}

void Profiler::printReport() const
{
#if VELODB_ENABLE_PROFILING
    fmt::println("\n=== Performance Profile Report ===");
    fmt::println("{:<40}{:>10}{:>15}{:>15}{:>15}{:>15}",
                 "Operation",
                 "Calls",
                 "Total (ms)",
                 "Avg (μs)",
                 "Min (μs)",
                 "Max (μs)");
    fmt::println("{:-^110}", "");

    for (const auto& [name, times] : timings_) {
        auto total_ms = total_time_.at(name) / 1000.0;
        auto avg_us = total_time_.at(name) / call_count_.at(name);
        auto min_us = *std::min_element(times.begin(), times.end());
        auto max_us = *std::max_element(times.begin(), times.end());

        fmt::println("{:<40} {:>10}{:>15.3f}{:>15}{:>15}{:>15}",
                     name,
                     call_count_.at(name),
                     total_ms,
                     avg_us,
                     min_us,
                     max_us);
    }
    fmt::println("================================\n");
#endif // VELODB_ENABLE_PROFILING
}

void Profiler::reset()
{
#if VELODB_ENABLE_PROFILING
    timings_.clear();
    total_time_.clear();
    call_count_.clear();
#endif // VELODB_ENABLE_PROFILING
}

std::vector<std::pair<std::string, long long>> Profiler::getHotSpots() const
{
#if VELODB_ENABLE_PROFILING
    std::vector<std::pair<std::string, long long>> hot_spots;
    for (const auto& [name, total] : total_time_) {
        hot_spots.emplace_back(name, total);
    }
    std::sort(hot_spots.begin(), hot_spots.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    return hot_spots;
#else
    return {};
#endif // VELODB_ENABLE_PROFILING
}

ScopedTimer::ScopedTimer(const std::string& name)
#if VELODB_ENABLE_PROFILING
    : name_(name)
    , start_(std::chrono::high_resolution_clock::now())
#else
#endif // VELODB_ENABLE_PROFILING
{
}

ScopedTimer::~ScopedTimer()
{
#if VELODB_ENABLE_PROFILING
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_);
    Profiler::getInstance().addTiming(name_, duration.count());
#endif // VELODB_ENABLE_PROFILING
}

} // namespace velodb
