add_library(velodb_common OBJECT exception.cpp)

target_include_directories(
  velodb_common PUBLIC ${CMAKE_SOURCE_DIR}/include
                       ${CMAKE_SOURCE_DIR}/thirdparty/backward-cpp)

target_link_libraries(velodb_common backward fmt::fmt)

set(ALL_OBJS
    ${ALL_OBJS} $<TARGET_OBJECTS:velodb_common>
    PARENT_SCOPE)
