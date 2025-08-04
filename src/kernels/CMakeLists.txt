add_library(velodb_kernels OBJECT ${OPERATOR_SOURCES} warmup.cu)

target_link_libraries(velodb_kernels ${DEPENDENCY_LIBS})

set(ALL_OBJS
    ${ALL_OBJS} $<TARGET_OBJECTS:velodb_kernels>
    PARENT_SCOPE)
