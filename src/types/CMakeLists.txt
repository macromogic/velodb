add_library(velodb_types OBJECT data_type.cpp type_checker.cpp value.cpp)

target_link_libraries(velodb_types ${DEPENDENCY_LIBS})

set(ALL_OBJS
    ${ALL_OBJS} $<TARGET_OBJECTS:velodb_types>
    PARENT_SCOPE)
