# TPC-H CMake Configuration

# Add data subdirectories
add_subdirectory(data)
add_subdirectory(harness)

# TPC-H benchmark library (combines all components)
add_library(
  tpch_lib
  # Sources will be added by subdirectories
)

target_link_libraries(tpch_lib PUBLIC tpch_data tpch_harness
                                      benchmark_infrastructure fmt::fmt)

target_include_directories(tpch_lib PUBLIC ${CMAKE_SOURCE_DIR}/include
                                           ${CMAKE_SOURCE_DIR}/benchmark)
