add_executable(test_data test_data_types.cpp test_type_checker.cpp
                         test_value.cpp test_bit_vector.cpp ../test_main.cpp)

target_link_libraries(test_data ${ALL_OBJS} ${DEPENDENCY_LIBS} GTest::GTest
                      GTest::Main pthread)

set_property(TARGET test_data PROPERTY CXX_STANDARD 17)
set_property(TARGET test_data PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET test_data PROPERTY RUNTIME_OUTPUT_DIRECTORY
                                       ${CMAKE_BINARY_DIR}/tests)

target_compile_options(test_data PRIVATE -Wall -Wextra -g)

add_test(NAME TestData COMMAND test_data)
set_tests_properties(TestData PROPERTIES TIMEOUT 20)
