# Infrastructure CMake Configuration

# Performance monitor library
add_library(benchmark_infrastructure performance_monitor.cpp)

target_include_directories(
  benchmark_infrastructure PUBLIC ${CMAKE_SOURCE_DIR}/include
                                  ${CMAKE_SOURCE_DIR}/benchmark)

target_link_libraries(benchmark_infrastructure fmt::fmt)

# Make sure we can find the headers
target_compile_features(benchmark_infrastructure PUBLIC cxx_std_17)
