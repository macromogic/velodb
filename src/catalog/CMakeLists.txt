add_library(velodb_catalog OBJECT
    schema.cpp
    table.cpp
    catalog.cpp
    mock_catalog_builder.cpp)

target_include_directories(velodb_catalog PRIVATE
    ${CMAKE_SOURCE_DIR}/thirdparty/sql-parser/src)

set(ALL_OBJS
    ${ALL_OBJS} $<TARGET_OBJECTS:velodb_catalog>
    PARENT_SCOPE)
