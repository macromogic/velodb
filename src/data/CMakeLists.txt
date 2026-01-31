add_library(
  velodb_data OBJECT bit_vector.cpp data_type.cpp type_checker.cpp value.cpp
                     oblivious_table.cpp oblivious_table_manager.cpp)

target_link_libraries(velodb_data ${DEPENDENCY_LIBS})

set(ALL_OBJS
    ${ALL_OBJS} $<TARGET_OBJECTS:velodb_data>
    PARENT_SCOPE)
