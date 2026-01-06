#pragma once

#include <string>
#include <vector>
#if VELODB_ENABLE_PROFILING
#include <chrono>
#include <map>
#endif // VELODB_ENABLE_PROFILING

namespace velodb {

class Profiler {
public:
    static Profiler& getInstance()
    {
        static Profiler instance;
        return instance;
    }

    void addTiming(const std::string&, long long);
    void printReport() const;
    void reset();

    std::vector<std::pair<std::string, long long>> getHotSpots() const;
#if VELODB_ENABLE_PROFILING
private:
    std::map<std::string, std::vector<long long>> timings_;
    std::map<std::string, long long> total_time_;
    std::map<std::string, int> call_count_;
#endif // VELODB_ENABLE_PROFILING
};

class ScopedTimer {
public:
    explicit ScopedTimer(const std::string& name);
    ~ScopedTimer();
#if VELODB_ENABLE_PROFILING
private:
    std::string name_;
    std::chrono::high_resolution_clock::time_point start_;
#endif // VELODB_ENABLE_PROFILING
};

// Convenience macros for profiling scopes
#if VELODB_ENABLE_PROFILING
#define PROFILE_SCOPE(name) ScopedTimer _timer(name)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCTION__)
#else
#define PROFILE_SCOPE(name)                                                                                            \
    do {                                                                                                               \
        (void)(name);                                                                                                  \
    } while (0)
#define PROFILE_FUNCTION()                                                                                             \
    do {                                                                                                               \
    } while (0)
#endif // VELODB_ENABLE_PROFILING

} // namespace velodb
