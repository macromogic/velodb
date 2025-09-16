# TPC-H Data Components

add_subdirectory(schemas)
add_subdirectory(loaders)

# TPC-H data library
add_library(
  tpch_data
  # Sources will be populated by subdirectories
)

target_link_libraries(tpch_data PUBLIC tpch_schemas tpch_loaders)
