add_library(velodb_catalog OBJECT
    schema.cpp
    column.cpp
    table.cpp
    catalog.cpp
    tuple.cpp
    mock_catalog_builder.cpp
)

target_include_directories(velodb_catalog PRIVATE
    ${CMAKE_SOURCE_DIR}/thirdparty/sql-parser/src
    ${CMAKE_SOURCE_DIR}/thirdparty/backward-cpp
)

set(ALL_OBJS
    ${ALL_OBJS} $<TARGET_OBJECTS:velodb_catalog>
    PARENT_SCOPE
)
