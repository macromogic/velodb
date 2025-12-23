# TPC-H Benchmark CMake Configuration

# Define benchmark sources directly (simpler approach)
set(BENCHMARK_SOURCES
    infrastructure/performance_monitor.cpp tpch/data/schemas/tpch_schemas.cpp
    tpch/data/loaders/tpch_data_loader.cpp tpch/harness/benchmark_runner.cpp
    ${CMAKE_SOURCE_DIR}/src/velodb.cpp # Add Database class implementation
)

# TPC-H benchmark library
add_library(tpch_benchmark_lib ${BENCHMARK_SOURCES})

target_include_directories(
  tpch_benchmark_lib
  PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/benchmark
         ${CMAKE_BINARY_DIR}/include)

target_link_libraries(tpch_benchmark_lib ${ALL_OBJS} ${DEPENDENCY_LIBS}
                      fmt::fmt)

# TPC-H benchmark executable
add_executable(tpch_benchmark tpch/main.cpp)

target_link_libraries(tpch_benchmark tpch_benchmark_lib argparse::argparse)

# Custom targets for running benchmarks
add_custom_target(
  generate_tpch_data
  COMMAND ${CMAKE_SOURCE_DIR}/benchmark/generate_data.sh 0.01
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Generating TPC-H data (Scale Factor: 0.01)")

add_custom_target(
  run_quick_benchmark
  COMMAND $<TARGET_FILE:tpch_benchmark> --quick --verbose
  DEPENDS tpch_benchmark generate_tpch_data
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Running quick TPC-H benchmark")

# Install targets
install(TARGETS tpch_benchmark RUNTIME DESTINATION bin COMPONENT benchmark)

# Create data directory structure
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/benchmark/data)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/benchmark/results)
