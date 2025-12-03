add_executable(
  test_query test_filter_complex.cpp test_filter.cpp test_limit.cpp
             test_order_by.cpp test_join_execution.cpp ../test_main.cpp)

target_link_libraries(test_query ${ALL_OBJS} ${DEPENDENCY_LIBS} GTest::GTest
                      GTest::Main pthread)

set_property(TARGET test_query PROPERTY CXX_STANDARD 17)
set_property(TARGET test_query PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET test_query PROPERTY RUNTIME_OUTPUT_DIRECTORY
                                        ${CMAKE_BINARY_DIR}/tests)

target_compile_options(test_query PRIVATE -Wall -Wextra -g)

add_test(NAME TestQuery COMMAND test_query)
