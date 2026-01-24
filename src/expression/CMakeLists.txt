add_library(
  velodb_expression OBJECT
  expression.cpp constant_expression.cpp column_ref_expression.cpp
  comparison_expression.cpp logical_expression.cpp arithmetic_expression.cpp)

target_include_directories(
  velodb_expression PRIVATE ${CMAKE_SOURCE_DIR}/thirdparty/sql-parser/src
                            ${CMAKE_SOURCE_DIR}/thirdparty/backward-cpp)

target_link_libraries(velodb_expression fmt::fmt)

set(ALL_OBJS
    ${ALL_OBJS} $<TARGET_OBJECTS:velodb_expression>
    PARENT_SCOPE)
