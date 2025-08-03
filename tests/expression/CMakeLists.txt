add_executable(test_expression
    test_expression.cpp
    ../test_main.cpp
)

target_link_libraries(test_expression
    ${ALL_OBJS}
    sqlparser
    backward
    fmt::fmt
    GTest::GTest
    GTest::Main
    pthread
)

set_property(TARGET test_expression PROPERTY CXX_STANDARD 17)
set_property(TARGET test_expression PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET test_expression PROPERTY RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)

target_compile_options(test_expression PRIVATE -Wall -Wextra -g)

add_test(NAME TestExpression COMMAND test_expression)
