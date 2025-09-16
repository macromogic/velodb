# TPC-H Schemas

add_library(tpch_schemas tpch_schemas.cpp)

target_include_directories(tpch_schemas PUBLIC ${CMAKE_SOURCE_DIR}/include
                                               ${CMAKE_SOURCE_DIR}/benchmark)

target_link_libraries(tpch_schemas fmt::fmt)
