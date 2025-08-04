cmake_minimum_required(VERSION 3.15)

# Project configuration
project(
  velodb
  VERSION 0.0.1
  DESCRIPTION "GPU-accelerated Object Database"
  LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate compile_commands.json for IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type configuration
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Generate version header from template
configure_file("${CMAKE_SOURCE_DIR}/include/velodb_version.hpp.in"
               "${CMAKE_BINARY_DIR}/include/velodb_version.hpp" @ONLY)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_BINARY_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)

# Dependencies
include(ExternalProject)

# Build sql-parser as external project Set build mode based on CMAKE_BUILD_TYPE
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

# Configure clean target to include thirdparty libraries
set_property(
  DIRECTORY
  APPEND
  PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
           "${CMAKE_SOURCE_DIR}/thirdparty/sql-parser/libsqlparser.a"
           "${CMAKE_SOURCE_DIR}/thirdparty/sql-parser/libsqlparser.so")

# Add custom target to clean sql-parser (can be called with ninja
# clean-sql-parser)
add_custom_target(
  clean-sql-parser
  COMMAND make clean
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/thirdparty/sql-parser
  COMMENT "Cleaning sql-parser library")

# Add custom target that cleans everything including thirdparty
add_custom_target(
  clean-all
  COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target clean
  COMMAND make clean
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/thirdparty/sql-parser
  COMMENT "Cleaning project and all thirdparty libraries")

# Dependencies
add_library(sqlparser STATIC IMPORTED)
set_target_properties(
  sqlparser PROPERTIES IMPORTED_LOCATION
                       ${CMAKE_SOURCE_DIR}/thirdparty/sql-parser/libsqlparser.a)
add_dependencies(sqlparser sql-parser)
add_subdirectory(thirdparty/argparse)
add_subdirectory(thirdparty/backward-cpp)
add_subdirectory(thirdparty/fmt)

# Project source files
add_subdirectory(src/common)
add_subdirectory(src/types)
add_subdirectory(src/catalog)
add_subdirectory(src/execution)
add_subdirectory(src/expression)
add_subdirectory(src/operator)
add_subdirectory(src/planner)

# Tests
option(BUILD_TESTS "Build test programs" ON)
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

set(SOURCES src/main.cpp src/velodb.cpp ${ALL_OBJS})

# Create main executable
add_executable(velodb ${SOURCES})
add_dependencies(velodb sql-parser)
add_dependencies(velodb fmt::fmt)

# Include directories for sql-parser
target_include_directories(
  velodb PRIVATE ${CMAKE_SOURCE_DIR}/thirdparty/sql-parser/src)

# Link libraries
target_link_libraries(velodb sqlparser argparse::argparse backward fmt::fmt)

# Set executable properties
set_target_properties(
  velodb PROPERTIES OUTPUT_NAME "velodb" RUNTIME_OUTPUT_DIRECTORY
                                         ${CMAKE_BINARY_DIR}/bin)

# Installation configuration
install(TARGETS velodb RUNTIME DESTINATION bin)

# Install headers
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
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
