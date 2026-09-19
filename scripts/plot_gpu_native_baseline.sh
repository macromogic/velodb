#!/bin/bash -eu

SCRIPTS_DIR="$(realpath "$(dirname "$0")")"
BASE_DIR="$(realpath "$(dirname "$0")/..")"
BENCHMARK_DIR="$BASE_DIR/benchmark_tpch"
LATEST_VELODB_CSV=$(ls -t "$BENCHMARK_DIR/results/sf_50"/*.csv 2>/dev/null | head -1)

VELODB_CSV="${VELODB_CSV:-$LATEST_VELODB_CSV}"
if [ ! -n "$VELODB_CSV" ]; then
    echo "Error: No benchmark results found in $BENCHMARK_DIR/results/sf_1"
    exit 1
fi

GPU_NATIVE_CSV="$BENCHMARK_DIR/results/gpu_native_results.csv"

# Collect benchmark results
COMBINED_CSV="$BENCHMARK_DIR/results/gpu_benchmark_results.csv"
echo "Query,Iteration,Engine,Time" > "$COMBINED_CSV"
tail -n +2 "$VELODB_CSV" | awk -F',' '{print $1","$3",VelODB,"$4}' >> "$COMBINED_CSV"
tail -n +2 "$GPU_NATIVE_CSV" | awk -F',' '{if ($1 != "TOTAL") {q=$1; sub(/^Q0+/, "Q", q); print q",1,Native,"$2}}' >> "$COMBINED_CSV"

# Plot benchmark results
source "$SCRIPTS_DIR/ensure_conda_env.sh"
FIGURE_DIR="$BASE_DIR/figures"
mkdir -p "$FIGURE_DIR"
BENCHMARK_PLOT="$FIGURE_DIR/benchmark_gpu_native.pdf"
python3 "$SCRIPTS_DIR/plot_bars.py" \
    -i "$COMBINED_CSV" \
    -o "$BENCHMARK_PLOT" \
    --ylabel "Time (ms)" \
    --bar-labels \
    --rotate-labels \
    --figsize 6,4 \
    --base-hue "Native" \
    --log-scale \
    --ylim-scale 5
