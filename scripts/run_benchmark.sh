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
ALL_SF=(0.01 0.1 1)
ALL_QUERIES=1,3,4,5,6,10,12,14,15,17,18,19
BUILD_PRESET="release"

# Parse command line arguments
function usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -d, --benchmark-dir DIR   Set benchmark directory (default: \$BASE_DIR/benchmark_tpch)"
    echo "  -i, --iterations N        Set number of iterations (default: 3)"
    echo "  -q, --queries LIST        Comma-separated list of TPC-H query numbers to run (default: all queries)"
    echo "  -s, --scale-factors LIST  Comma-separated list of scale factors to run (default: 0.01,0.1,1)"
    echo "  -p, --with-profiling      Run performance breakdown with profiling"
    echo "  --skip-benchmark          Skip running VelODB benchmarks"
    echo "  --skip-baseline           Skip running baseline (Pandas) benchmarks"
    echo "  -D, --with-debug-info     Include debug information during compilation"
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
        -q|--queries)
            ALL_QUERIES="$2"
            shift 2
            ;;
        -s|--scale-factors)
            IFS=',' read -ra ALL_SF <<< "$2"
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
        -D|--with-debug-info)
            BUILD_PRESET="relwithdebinfo"
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

function run_bench() {
    local SF=$1
    local QUERIES=$2
    shift 2
    local ADDITIONAL_ARGS="$@"
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
        -j hash \
        --export-format csv \
        --results-dir "$BENCHMARK_DIR/results/sf_$SF" \
        $ADDITIONAL_ARGS
}

function run_duckdb() {
    local SF=$1
    local QUERIES=$2
    local OUT_CSV=$3
    local TIMEOUT=$((SF * 60))
    shift 3
    local ADDITIONAL_ARGS="$@"
    DATA_DIR="$BENCHMARK_DIR/data/sf_$SF"
    source "$SCRIPTS_DIR/ensure_conda_env.sh"
    python3 -u "$SCRIPTS_DIR/bench_duckdb.py" \
        -i "$ITERATIONS" \
        -t "$TIMEOUT" \
        --data-dir "$DATA_DIR" \
        -q "$QUERIES" \
        -o "$OUT_CSV" \
        $ADDITIONAL_ARGS
}

function run_gpu_native() {
    local SF=$1
    local OUT_CSV=$2
    DATA_DIR="$BENCHMARK_DIR/data/sf_$SF"
    stdbuf -oL -eL $BUILD_DIR/bin/gpu_native_benchmark \
        -d "$DATA_DIR" \
        -w 1 -r "$ITERATIONS" \
        -o "$OUT_CSV"
}

# Setup benchmark directory
mkdir -p "$BENCHMARK_DIR/data"
echo "*" > "$BENCHMARK_DIR/data/.gitignore"

if [ "$SKIP_BENCHMARK" = false ] || [ "$SKIP_BASELINE" = false ]; then
    build "$BUILD_PRESET-no-profiling"
fi

# Run benchmarks
if [ "$SKIP_BENCHMARK" = false ]; then
    for SF in "${ALL_SF[@]}"; do
        run_bench "$SF" "$ALL_QUERIES"
    done
else
    echo "Skipping VelODB benchmarks (--skip-benchmark)"
fi

# Run baselines
if [ "$SKIP_BASELINE" = false ]; then
    for SF in "${ALL_SF[@]}"; do
        run_duckdb "$SF" "$ALL_QUERIES" "$BENCHMARK_DIR/results/duckdb_results_sf${SF}.csv" --single-threaded
        run_duckdb "$SF" "$ALL_QUERIES" "$BENCHMARK_DIR/results/duckdb16_results_sf${SF}.csv"
    done
else
    echo "Skipping baseline benchmarks (--skip-baseline)"
fi

# Performance breakdown with profiling
if [ "$WITH_PROFILING" = true ]; then
    build "$BUILD_PRESET"
    PROFILE_LOG="$BENCHMARK_DIR/results/profile_breakdown.log"
    mkdir -p "$(dirname "$PROFILE_LOG")"
    echo "Profiling output will be saved to $PROFILE_LOG"
    echo -n "" > "$PROFILE_LOG"
    for SF in "${ALL_SF[@]}"; do
        run_bench "$SF" "$ALL_QUERIES" --with-profiling --profile-per-query | tee -a "$PROFILE_LOG"
    done
fi
