add_executable(test_operator test_operator.cpp ../test_main.cpp)

target_link_libraries(test_operator ${ALL_OBJS} ${DEPENDENCY_LIBS} GTest::GTest
                      GTest::Main pthread)

set_property(TARGET test_operator PROPERTY CXX_STANDARD 17)
set_property(TARGET test_operator PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET test_operator PROPERTY RUNTIME_OUTPUT_DIRECTORY
                                           ${CMAKE_BINARY_DIR}/tests)

target_compile_options(test_operator PRIVATE -Wall -Wextra -g)

add_test(NAME TestOperator COMMAND test_operator)
set_tests_properties(TestOperator PROPERTIES TIMEOUT 15)
