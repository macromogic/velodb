#!/bin/bash -eu

SCRIPTS_DIR="$(realpath "$(dirname "$0")")"
BASE_DIR="$(realpath "$(dirname "$0")/..")"
BUILD_DIR="$BASE_DIR/build"

# Default values
BENCHMARK_DIR="$BASE_DIR/benchmark_tpch"
ITERATIONS=3
WITH_PROFILING=false
SKIP_BENCHMARK=false
SKIP_BASELINE=false
ALL_QUERIES=1,3,4,5,6,10,12,14,15,17,18,19

# Parse command line arguments
function usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -d, --benchmark-dir DIR   Set benchmark directory (default: \$BASE_DIR/benchmark_tpch)"
    echo "  -i, --iterations N        Set number of iterations (default: 3)"
    echo "  -p, --with-profiling      Run performance breakdown with profiling"
    echo "  --skip-benchmark          Skip running VelODB benchmarks"
    echo "  --skip-baseline           Skip running baseline (Pandas) benchmarks"
    echo "  -h, --help                Show this help message"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--benchmark-dir)
            BENCHMARK_DIR="$(realpath "$2")"
            shift 2
            ;;
        -i|--iterations)
            ITERATIONS="$2"
            shift 2
            ;;
        -p|--with-profiling)
            WITH_PROFILING=true
            shift
            ;;
        --skip-benchmark)
            SKIP_BENCHMARK=true
            shift
            ;;
        --skip-baseline)
            SKIP_BASELINE=true
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Unknown option: $1"
            usage
            ;;
    esac
done

function build() {
    local BUILD_PRESET=${1:-release-no-profiling}
    pushd "$BASE_DIR" > /dev/null
    cmake --preset $BUILD_PRESET
    cmake --build --preset $BUILD_PRESET
    popd > /dev/null
}

function ensure_conda_env() {
    if ! command -v conda &> /dev/null; then
        echo "Error: conda is not available"
        exit 1
    fi

    if ! conda env list | grep -q "^velodb "; then
        echo "Creating conda environment 'velodb'..."
        conda create -y -n velodb python=3.12
        conda run -n velodb pip install seaborn pandas==2.3.3 duckdb
    fi

    eval "$(conda shell.bash hook)"
    conda activate velodb
}

function run_bench() {
    local SF=$1
    local QUERIES=$2
    local ADDITIONAL_ARGS=${3:-}
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
        -s "$SF" \
        -d "$DATA_DIR" \
        -i "$ITERATIONS" \
        -q "$QUERIES" \
        --export-format csv \
        --results-dir "$BENCHMARK_DIR/results/sf_$SF" \
        $ADDITIONAL_ARGS
}

# Setup benchmark directory
mkdir -p "$BENCHMARK_DIR/data"
echo "*" > "$BENCHMARK_DIR/data/.gitignore"

# Run benchmarks
if [ "$SKIP_BENCHMARK" = false ]; then
    build
    run_bench 0.01 "$ALL_QUERIES"
    run_bench 0.1 "$ALL_QUERIES"
    run_bench 1 "$ALL_QUERIES"
else
    echo "Skipping VelODB benchmarks (--skip-benchmark)"
fi

VELODB_CSV=$(ls -t "$BENCHMARK_DIR/results/sf_1"/*.csv 2>/dev/null | head -1)
if [ ! -n "$VELODB_CSV" ]; then
    echo "Error: No benchmark results found in $BENCHMARK_DIR/results/sf_1"
    exit 1
fi

# Run baselines
if [ "$SKIP_BASELINE" = false ]; then
    ensure_conda_env
    PANDAS_CSV="$BENCHMARK_DIR/results/pandas_results.csv"
    python3 -u "$SCRIPTS_DIR/bench_pandas.py" \
        -i "$ITERATIONS" \
        -t 20 \
        --data-dir "$BENCHMARK_DIR/data/sf_1" \
        -q "$ALL_QUERIES" \
        -o "$PANDAS_CSV"
else
    echo "Skipping baseline benchmarks (--skip-baseline)"
    PANDAS_CSV="$BENCHMARK_DIR/results/pandas_results.csv"
fi

# Collect benchmark results
COMBINED_CSV="$BENCHMARK_DIR/results/benchmark_results.csv"
echo "Query,Iteration,Engine,Time(ms)" > "$COMBINED_CSV"
tail -n +2 "$VELODB_CSV" | awk -F',' '{print $1","$3",VelODB,"$4}' >> "$COMBINED_CSV"
tail -n +2 "$PANDAS_CSV" | awk -F',' '{print $1","$2",Pandas,"$3}' >> "$COMBINED_CSV"
echo "Combined results saved to: $COMBINED_CSV"

# Plot benchmark results
BENCHMARK_PLOT="$BENCHMARK_DIR/results/benchmark_comparison.png"
python3 "$SCRIPTS_DIR/plot_benchmark.py" -i "$COMBINED_CSV" -o "$BENCHMARK_PLOT"

# Performance breakdown with profiling
if [ "$WITH_PROFILING" = true ]; then
    build release
    IFS=',' read -ra QUERY_ARRAY <<< "$ALL_QUERIES"
    for QUERY_ID in "${QUERY_ARRAY[@]}"; do
        run_bench 1 "$QUERY_ID" "--with-profiling"
    done
fi
