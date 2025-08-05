add_library(
  velodb_catalog OBJECT
  schema.cpp
  column.cpp
  table.cpp
  catalog.cpp
  tuple.cpp
  mock_catalog_builder.cpp
  execution_context.cpp)

target_link_libraries(velodb_catalog ${DEPENDENCY_LIBS})

set(ALL_OBJS
    ${ALL_OBJS} $<TARGET_OBJECTS:velodb_catalog>
    PARENT_SCOPE)
