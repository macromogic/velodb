cmake_minimum_required(VERSION 3.15)

# CUDA configuration Set CUDA compiler path explicitly to ensure VS Code can
# find it
if(NOT CMAKE_CUDA_COMPILER)
  find_program(
    CMAKE_CUDA_COMPILER nvcc
    PATHS /usr/local/cuda/bin
    NO_DEFAULT_PATH)
endif()

set(CMAKE_CUDA_ARCHITECTURES "70;75;80;86;89;90")

# Project configuration
project(
  velodb
  VERSION 0.0.1
  DESCRIPTION "GPU-accelerated Object Database"
  LANGUAGES CXX CUDA)

find_package(CUDAToolkit REQUIRED)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# Generate compile_commands.json for IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type configuration
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g3 -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CUDA_FLAGS_DEBUG
    "-g3 -G -Xcompiler -Wall -Xcompiler -Wextra -Xcompiler -Werror")
set(CMAKE_CUDA_FLAGS_RELEASE "-O3 --use_fast_math")

# Generate version header from template
configure_file("${CMAKE_SOURCE_DIR}/include/velodb_version.hpp.in"
               "${CMAKE_BINARY_DIR}/include/velodb_version.hpp" @ONLY)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_BINARY_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CUDAToolkit_INCLUDE_DIRS})

# Dependencies
include(ExternalProject)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(SQL_PARSER_BUILD_COMMAND make static=yes mode=debug relaxed_build=on)
else()
  set(SQL_PARSER_BUILD_COMMAND make static=yes)
endif()
ExternalProject_Add(
  sql-parser
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/thirdparty/sql-parser
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ${SQL_PARSER_BUILD_COMMAND}
  INSTALL_COMMAND ""
  BUILD_BYPRODUCTS ${CMAKE_SOURCE_DIR}/thirdparty/sql-parser/libsqlparser.a
  BUILD_IN_SOURCE 1)
set_property(
  DIRECTORY
  APPEND
  PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
           "${CMAKE_SOURCE_DIR}/thirdparty/sql-parser/libsqlparser.a"
           "${CMAKE_SOURCE_DIR}/thirdparty/sql-parser/libsqlparser.so")
add_library(sqlparser STATIC IMPORTED)
set_target_properties(
  sqlparser PROPERTIES IMPORTED_LOCATION
                       ${CMAKE_SOURCE_DIR}/thirdparty/sql-parser/libsqlparser.a)
set(FMT_SYSTEM_HEADERS
    ON
    CACHE BOOL "" FORCE)
add_dependencies(sqlparser sql-parser)
add_subdirectory(thirdparty/argparse)
add_subdirectory(thirdparty/backward-cpp)
add_subdirectory(thirdparty/fmt)
set(DEPENDENCY_LIBS sqlparser argparse::argparse backward fmt::fmt CUDA::cudart)
include_directories(${CMAKE_SOURCE_DIR}/thirdparty/sql-parser/src
                    ${CMAKE_SOURCE_DIR}/thirdparty/backward-cpp)

# Options
option(BUILD_TESTS "Build test programs" ON)
option(ENABLE_PROFILING "Enable performance profiling" ON)

# Profiling configuration (must be before add_subdirectory calls)
if(ENABLE_PROFILING)
  add_compile_definitions(VELODB_ENABLE_PROFILING=1)
  message(STATUS "Performance profiling: ENABLED")
else()
  add_compile_definitions(VELODB_ENABLE_PROFILING=0)
  message(STATUS "Performance profiling: DISABLED")
endif()

# Project source files
add_subdirectory(src/common)
add_subdirectory(src/data)
add_subdirectory(src/catalog)
add_subdirectory(src/execution)
add_subdirectory(src/cuda)
add_subdirectory(src/expression)
add_subdirectory(src/operator)
add_subdirectory(src/planner)

set(SOURCES src/main.cpp src/velodb.cpp ${ALL_OBJS})

# Create main executable
add_executable(velodb ${SOURCES})
target_link_libraries(velodb ${DEPENDENCY_LIBS})

# Tests
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# Set executable properties
set_target_properties(
  velodb PROPERTIES OUTPUT_NAME "velodb" RUNTIME_OUTPUT_DIRECTORY
                                         ${CMAKE_BINARY_DIR}/bin)

# Installation configuration
install(TARGETS velodb RUNTIME DESTINATION bin)
install(
  DIRECTORY ${CMAKE_SOURCE_DIR}/include/
  DESTINATION include
  FILES_MATCHING
  PATTERN "*.h"
  PATTERN "*.hpp")

# Print configuration summary
message(STATUS "")
message(STATUS "Configuration Summary:")
message(STATUS "  Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build tests: ${BUILD_TESTS}")
message(STATUS "  Profiling: ${ENABLE_PROFILING}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
