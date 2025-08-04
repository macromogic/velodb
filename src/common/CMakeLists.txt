add_library(velodb_common OBJECT exception.cpp)

target_link_libraries(velodb_common ${DEPENDENCY_LIBS})

set(ALL_OBJS
    ${ALL_OBJS} $<TARGET_OBJECTS:velodb_common>
    PARENT_SCOPE)
