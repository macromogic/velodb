add_library(common
    traced_exception.cpp
)

target_include_directories(common PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/thirdparty/backward-cpp
)

target_link_libraries(common
    backward
)
