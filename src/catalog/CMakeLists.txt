add_library(velodb_catalog OBJECT
    schema.cpp
    table.cpp
    catalog.cpp)

set(ALL_OBJS
    ${ALL_OBJS} $<TARGET_OBJECTS:velodb_catalog>
    PARENT_SCOPE)
