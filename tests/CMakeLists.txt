# Unit tests for VeloDB

# Find Google Test
find_package(GTest REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/thirdparty/sql-parser/src)
include_directories(${CMAKE_SOURCE_DIR}/backward-cpp)

add_subdirectory(catalog)
add_subdirectory(expression)
add_subdirectory(filter)
add_subdirectory(operator)
add_subdirectory(planner)
add_subdirectory(types)