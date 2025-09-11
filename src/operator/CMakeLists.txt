add_library(
  velodb_operator OBJECT
  abstract_operator.cpp
  filter_compaction_operator.cpp
  limit_operator.cpp
  merge_sort_join_operator.cpp
  projection_operator.cpp
  seq_scan_operator.cpp
  sort_operator.cpp)

target_link_libraries(velodb_operator ${DEPENDENCY_LIBS})

set(ALL_OBJS
    ${ALL_OBJS} $<TARGET_OBJECTS:velodb_operator>
    PARENT_SCOPE)
