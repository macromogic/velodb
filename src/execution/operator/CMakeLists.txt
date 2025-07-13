# Operator implementation files
set(OPERATOR_SOURCES
    abstract_operator.cpp
    result_set.cpp
    seq_scan_operator.cpp
    projection_operator.cpp
    filter_operator.cpp
    nested_loop_join_operator.cpp
    hash_join_operator.cpp
    merge_sort_join_operator.cpp
    sort_operator.cpp
    limit_operator.cpp
)

# Add operator sources to parent scope with proper prefix
foreach(source ${OPERATOR_SOURCES})
    list(APPEND OPERATOR_SOURCES_PREFIXED "operator/${source}")
endforeach()
set(OPERATOR_SOURCES ${OPERATOR_SOURCES_PREFIXED} PARENT_SCOPE)
