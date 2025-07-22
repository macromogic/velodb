add_library(velodb_planner OBJECT
    abstract_plan_node.cpp
    scan_filter_plan_node.cpp
    projection_plan_node.cpp
    nested_loop_join_plan_node.cpp
    hash_join_plan_node.cpp
    merge_sort_join_plan_node.cpp
    sort_plan_node.cpp
    limit_plan_node.cpp
    query_planner.cpp
    execution_context.cpp
    plan_visualizer.cpp)

target_include_directories(velodb_planner PRIVATE
    ${CMAKE_SOURCE_DIR}/thirdparty/sql-parser/src
    ${CMAKE_SOURCE_DIR}/thirdparty/backward-cpp)

set(ALL_OBJS
    ${ALL_OBJS} $<TARGET_OBJECTS:velodb_planner>
    PARENT_SCOPE)
