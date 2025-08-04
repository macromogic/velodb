add_executable(test_catalog test_adaptive_catalog.cpp test_catalog.cpp
                            test_schema.cpp test_table.cpp ../test_main.cpp)

target_link_libraries(test_catalog ${ALL_OBJS} ${DEPENDENCY_LIBS} GTest::GTest
                      GTest::Main pthread)

set_property(TARGET test_catalog PROPERTY CXX_STANDARD 17)
set_property(TARGET test_catalog PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET test_catalog PROPERTY RUNTIME_OUTPUT_DIRECTORY
                                          ${CMAKE_BINARY_DIR}/tests)

target_compile_options(test_catalog PRIVATE -Wall -Wextra -g)

add_test(NAME TestCatalog COMMAND test_catalog)
