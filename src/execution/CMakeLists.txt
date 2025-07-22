# Include operator subdirectory
add_subdirectory(operator)

add_library(velodb_execution OBJECT
    ${OPERATOR_SOURCES}
    expression.cpp
    constant_expression.cpp
    column_ref_expression.cpp
    comparison_expression.cpp
    conjunction_expression.cpp
    arithmetic_expression.cpp
    cast_expression.cpp
    type_checker.cpp
    function_call_expression.cpp
    hash_table.cpp
    sorter.cpp
    execution_engine.cpp)

target_include_directories(velodb_execution PRIVATE
    ${CMAKE_SOURCE_DIR}/thirdparty/sql-parser/src
    ${CMAKE_SOURCE_DIR}/thirdparty/backward-cpp)

set(ALL_OBJS
    ${ALL_OBJS} $<TARGET_OBJECTS:velodb_execution>
    PARENT_SCOPE)
