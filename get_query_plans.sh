#!/bin/bash

for SQL in benchmark/tpch/queries/templates/*.sql; do
    QUERY_NAME=$(basename "$SQL" .sql)
    echo "Query plan for $QUERY_NAME"
    ./build/bin/velodb --verbose --tpch-catalog "$(cat $SQL)"
done
