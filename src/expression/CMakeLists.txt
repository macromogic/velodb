add_library(velodb_expression OBJECT
    expression.cpp
    constant_expression.cpp
    column_ref_expression.cpp
    comparison_expression.cpp
    conjunction_expression.cpp
    arithmetic_expression.cpp
    cast_expression.cpp
    function_call_expression.cpp
)

target_include_directories(velodb_expression PRIVATE
    ${CMAKE_SOURCE_DIR}/thirdparty/sql-parser/src
    ${CMAKE_SOURCE_DIR}/thirdparty/backward-cpp
)

set(ALL_OBJS
    ${ALL_OBJS} $<TARGET_OBJECTS:velodb_expression>
    PARENT_SCOPE
)
