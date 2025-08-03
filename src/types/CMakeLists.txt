add_library(velodb_types OBJECT
    data_type.cpp
    type_checker.cpp
    value.cpp
)

target_include_directories(velodb_types PRIVATE
    ${CMAKE_SOURCE_DIR}/thirdparty/backward-cpp
)

target_link_libraries(velodb_types
    fmt::fmt
)

set(ALL_OBJS
    ${ALL_OBJS} $<TARGET_OBJECTS:velodb_types>
    PARENT_SCOPE
)
