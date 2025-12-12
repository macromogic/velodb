# Define the test executable
add_executable(oblivious_primitives_test oblivious_primitives_test.cpp
                                         ../test_main.cpp)

# Link libraries
target_link_libraries(
  oblivious_primitives_test PRIVATE ${ALL_OBJS} ${DEPENDENCY_LIBS} GTest::gtest
                                    GTest::gtest_main CUDA::cudart pthread)

# Add test
add_test(NAME oblivious_primitives_test COMMAND oblivious_primitives_test)

add_executable(test_pools test_pools.cpp ../test_main.cpp)
target_link_libraries(test_pools PRIVATE ${ALL_OBJS} ${DEPENDENCY_LIBS}
                                         GTest::gtest GTest::gtest_main pthread)
set_property(TARGET test_pools PROPERTY CXX_STANDARD 17)
set_property(TARGET test_pools PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET test_pools PROPERTY RUNTIME_OUTPUT_DIRECTORY
                                        ${CMAKE_BINARY_DIR}/tests)
