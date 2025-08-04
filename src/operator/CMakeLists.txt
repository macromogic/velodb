add_library(
  velodb_operator OBJECT
  abstract_operator.cpp
  filter_compaction_operator.cpp
  hash_join_operator.cpp
  limit_operator.cpp
  merge_sort_join_operator.cpp
  nested_loop_join_operator.cpp
  projection_operator.cpp
  seq_scan_operator.cpp
  sort_operator.cpp)

target_include_directories(
  velodb_operator PRIVATE ${CMAKE_SOURCE_DIR}/thirdparty/sql-parser/src
                          ${CMAKE_SOURCE_DIR}/thirdparty/backward-cpp)

target_link_libraries(velodb_operator fmt::fmt)

set(ALL_OBJS
    ${ALL_OBJS} $<TARGET_OBJECTS:velodb_operator>
    PARENT_SCOPE)
