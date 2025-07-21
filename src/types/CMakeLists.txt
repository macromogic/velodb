add_library(velodb_types OBJECT
    data_type.cpp
    value.cpp)

target_include_directories(velodb_types PRIVATE
    ${CMAKE_SOURCE_DIR}/backward-cpp)

set(ALL_OBJS
    ${ALL_OBJS} $<TARGET_OBJECTS:velodb_types>
    PARENT_SCOPE)
