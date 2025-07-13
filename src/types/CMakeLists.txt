add_library(velodb_types OBJECT
    data_type.cpp
    value.cpp)

set(ALL_OBJS
    ${ALL_OBJS} $<TARGET_OBJECTS:velodb_types>
    PARENT_SCOPE)
