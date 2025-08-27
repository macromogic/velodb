add_library(velodb_cuda OBJECT ${OPERATOR_SOURCES} warmup.cu stream.cpp
                               stream_pool.cpp)

target_link_libraries(velodb_cuda ${DEPENDENCY_LIBS})

set(ALL_OBJS
    ${ALL_OBJS} $<TARGET_OBJECTS:velodb_cuda>
    PARENT_SCOPE)
