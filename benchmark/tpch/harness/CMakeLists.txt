# TPC-H Benchmark Harness

add_library(tpch_harness benchmark_runner.cpp)

target_include_directories(tpch_harness PUBLIC ${CMAKE_SOURCE_DIR}/include
                                               ${CMAKE_SOURCE_DIR}/benchmark)

target_link_libraries(tpch_harness tpch_loaders benchmark_infrastructure
                      fmt::fmt)
