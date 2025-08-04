# Unit tests for VeloDB

# Find Google Test
find_package(GTest REQUIRED)

add_subdirectory(catalog)
add_subdirectory(expression)
add_subdirectory(filter)
add_subdirectory(operator)
add_subdirectory(planner)
add_subdirectory(types)
