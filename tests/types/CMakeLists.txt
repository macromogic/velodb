add_executable(test_types
    test_data_types.cpp
    test_type_checker.cpp
    test_type_safety_demo.cpp
    test_value.cpp
    ../test_main.cpp
)

target_link_libraries(test_types
    ${ALL_OBJS}
    sqlparser
    backward
    fmt::fmt
    GTest::GTest
    GTest::Main
    pthread
)

set_property(TARGET test_types PROPERTY CXX_STANDARD 17)
set_property(TARGET test_types PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET test_types PROPERTY RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)

target_compile_options(test_types PRIVATE -Wall -Wextra -g)

add_test(NAME TestTypes COMMAND test_types)
