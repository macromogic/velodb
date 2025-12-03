add_library(
  velodb_planner OBJECT
  abstract_plan_node.cpp
  filter_compaction_plan_node.cpp
  limit_plan_node.cpp
  merge_sort_join_plan_node.cpp
  materialization_plan_node.cpp
  plan_visualizer.cpp
  projection_plan_node.cpp
  query_planner.cpp
  seq_scan_plan_node.cpp
  sort_plan_node.cpp)

target_link_libraries(velodb_planner ${DEPENDENCY_LIBS})

set(ALL_OBJS
    ${ALL_OBJS} $<TARGET_OBJECTS:velodb_planner>
    PARENT_SCOPE)
