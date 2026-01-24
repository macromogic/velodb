add_executable(test_planner test_plan_visualization.cpp test_planner.cpp
                            test_join_planner.cpp ../test_main.cpp)

target_link_libraries(test_planner ${ALL_OBJS} ${DEPENDENCY_LIBS} GTest::GTest
                      GTest::Main pthread)

set_property(TARGET test_planner PROPERTY CXX_STANDARD 17)
set_property(TARGET test_planner PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET test_planner PROPERTY RUNTIME_OUTPUT_DIRECTORY
                                          ${CMAKE_BINARY_DIR}/tests)

target_compile_options(test_planner PRIVATE -Wall -Wextra -g)

add_test(NAME TestPlanner COMMAND test_planner)
set_tests_properties(TestPlanner PROPERTIES TIMEOUT 20)
