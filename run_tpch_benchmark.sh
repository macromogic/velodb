#!/bin/bash -e

BASE_DIR="$(dirname "$0")"
BUILD_DIR="$BASE_DIR/build"
BENCHMARK_DIR="$BASE_DIR/benchmark_tpch"

ITERATIONS=3
QUERIES=1,4,6,12,14,15,17,19

BUILD_PRESET=release
pushd "$BASE_DIR" > /dev/null
cmake --preset $BUILD_PRESET
cmake --build --preset $BUILD_PRESET
popd > /dev/null

# if [ -d "$BENCHMARK_DIR" ]; then
#     rm -rf "$BENCHMARK_DIR"
# fi
mkdir -p "$BENCHMARK_DIR/data"
echo "*" > "$BENCHMARK_DIR/data/.gitignore"


function run_bench() {
    local SF=$1
    DATA_DIR="$BENCHMARK_DIR/data/sf_$SF"
    if [ ! -d "$DATA_DIR" ]; then
        echo "Generating TPC-H data for scale factor $SF..."
        mkdir -p "$DATA_DIR"
        bash "$BASE_DIR/benchmark/generate_data.sh" "$SF" "$DATA_DIR"
    else
        echo "TPC-H data for scale factor $SF already exists. Skipping generation."
    fi
    stdbuf -oL -eL $BUILD_DIR/benchmark/tpch_benchmark \
        -v \
        --with-profiling \
        -s "$SF" \
        -d "$DATA_DIR" \
        -i "$ITERATIONS" \
        -q "$QUERIES" \
        --export-format csv \
        --results-dir "$BENCHMARK_DIR/results/sf_$SF"
}

# run_bench 0.01
# run_bench 0.1
# run_bench 1
# run_bench 2
# run_bench 3

for QUERIES in 1 4 6 12 14 15 17 19; do
    run_bench 1 | tee profile_q${QUERIES}_sf1.1.log
done
