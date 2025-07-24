add_library(velodb_operator OBJECT
    abstract_operator.cpp
    scan_filter_operator.cpp
    projection_operator.cpp
    result_set.cpp
)

target_include_directories(velodb_operator PRIVATE
    ${CMAKE_SOURCE_DIR}/thirdparty/sql-parser/src
    ${CMAKE_SOURCE_DIR}/thirdparty/backward-cpp
)

set(ALL_OBJS
    ${ALL_OBJS} $<TARGET_OBJECTS:velodb_operator>
    PARENT_SCOPE
)
