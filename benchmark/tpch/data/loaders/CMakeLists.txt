# TPC-H Data Loaders

add_library(tpch_loaders tpch_data_loader.cpp)

target_include_directories(tpch_loaders PUBLIC ${CMAKE_SOURCE_DIR}/include
                                               ${CMAKE_SOURCE_DIR}/benchmark)

target_link_libraries(tpch_loaders tpch_schemas fmt::fmt)
