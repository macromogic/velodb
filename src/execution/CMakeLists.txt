add_library(velodb_execution OBJECT ${OPERATOR_SOURCES} execution_engine.cpp
                                    query_result.cpp)

target_link_libraries(velodb_execution ${DEPENDENCY_LIBS})

set(ALL_OBJS
    ${ALL_OBJS} $<TARGET_OBJECTS:velodb_execution>
    PARENT_SCOPE)
