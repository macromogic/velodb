# Unit tests for VeloDB

# Find Google Test
find_package(GTest REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/thirdparty/sql-parser/src)
include_directories(${CMAKE_SOURCE_DIR}/backward-cpp)

# Test executable
add_executable(velodb_tests
    test_main.cpp
    # test_catalog.cpp
    # test_data_types.cpp
    # test_execution_engine.cpp
    # test_expression.cpp
    # test_operator.cpp
    # test_planner.cpp
    # test_schema.cpp
    # test_table.cpp
    # test_type_checker.cpp
    # test_type_safety_demo.cpp
    # test_value.cpp
    # test_where_clause.cpp
    # test_plan_visualization.cpp
    # test_adaptive_catalog.cpp
)

# Link libraries
target_link_libraries(velodb_tests
    ${ALL_OBJS}
    sqlparser
    backward
    GTest::GTest
    GTest::Main
    pthread
)

# Add test to CTest
add_test(NAME VeloDBTests COMMAND velodb_tests)

# Set C++ standard
set_property(TARGET velodb_tests PROPERTY CXX_STANDARD 17)
set_property(TARGET velodb_tests PROPERTY CXX_STANDARD_REQUIRED ON)

# Compiler flags
target_compile_options(velodb_tests PRIVATE -Wall -Wextra -g)
