#!/bin/bash -e

BASE_DIR="$(dirname "$0")"
BUILD_DIR="$BASE_DIR/build"
BENCHMARK_DIR="$BASE_DIR/benchmark_tpch"

ITERATIONS=3
QUERIES=1,4,6,12,14,15,17,19

if [ -d "$BENCHMARK_DIR" ]; then
    rm -rf "$BENCHMARK_DIR"
fi
mkdir -p "$BENCHMARK_DIR/data"
echo "*" > "$BENCHMARK_DIR/data/.gitignore"
for SF in 0.01 0.1 1 2 3 4 5 10; do
    DATA_DIR="$BENCHMARK_DIR/data/sf_$SF"
    if [ ! -d "$DATA_DIR" ]; then
        echo "Generating TPC-H data for scale factor $SF..."
        mkdir -p "$DATA_DIR"
        bash "$BASE_DIR/benchmark/generate_data.sh" "$SF" "$DATA_DIR"
    else
        echo "TPC-H data for scale factor $SF already exists. Skipping generation."
    fi
    $BUILD_DIR/benchmark/tpch_benchmark -v -s "$SF" -d "$DATA_DIR" -i "$ITERATIONS" -q "$QUERIES" --export-format csv --results-dir "$BENCHMARK_DIR/results/sf_$SF"
done
