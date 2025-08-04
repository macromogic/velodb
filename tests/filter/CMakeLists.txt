add_executable(test_filter test_filter_complex.cpp test_filter.cpp
                           ../test_main.cpp)

target_link_libraries(
  test_filter
  ${ALL_OBJS}
  sqlparser
  backward
  fmt::fmt
  GTest::GTest
  GTest::Main
  pthread)

set_property(TARGET test_filter PROPERTY CXX_STANDARD 17)
set_property(TARGET test_filter PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET test_filter PROPERTY RUNTIME_OUTPUT_DIRECTORY
                                         ${CMAKE_BINARY_DIR}/tests)

target_compile_options(test_filter PRIVATE -Wall -Wextra -g)

add_test(NAME TestFilter COMMAND test_filter)
