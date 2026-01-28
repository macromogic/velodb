add_executable(test_expression test_expression.cpp test_expression_batch.cpp
                               ../test_main.cpp)

target_link_libraries(test_expression ${ALL_OBJS} ${DEPENDENCY_LIBS}
                      GTest::GTest GTest::Main pthread)

set_property(TARGET test_expression PROPERTY CXX_STANDARD 17)
set_property(TARGET test_expression PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET test_expression PROPERTY RUNTIME_OUTPUT_DIRECTORY
                                             ${CMAKE_BINARY_DIR}/tests)

target_compile_options(test_expression PRIVATE -Wall -Wextra -g)

add_test(NAME TestExpression COMMAND test_expression)
set_tests_properties(TestExpression PROPERTIES TIMEOUT 30)
