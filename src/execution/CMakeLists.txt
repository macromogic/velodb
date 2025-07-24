add_library(velodb_execution OBJECT
    ${OPERATOR_SOURCES}
    hash_table.cpp
    sorter.cpp
    execution_engine.cpp
)

target_include_directories(velodb_execution PRIVATE
    ${CMAKE_SOURCE_DIR}/thirdparty/sql-parser/src
    ${CMAKE_SOURCE_DIR}/thirdparty/backward-cpp
)

set(ALL_OBJS
    ${ALL_OBJS} $<TARGET_OBJECTS:velodb_execution>
    PARENT_SCOPE
)
