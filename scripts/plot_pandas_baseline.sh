#!/bin/bash -eu

SCRIPTS_DIR="$(realpath "$(dirname "$0")")"
BASE_DIR="$(realpath "$(dirname "$0")/..")"
BENCHMARK_DIR="$BASE_DIR/benchmark_tpch"

VELODB_CSV=$(ls -t "$BENCHMARK_DIR/results/sf_1"/*.csv 2>/dev/null | head -1)
if [ ! -n "$VELODB_CSV" ]; then
    echo "Error: No benchmark results found in $BENCHMARK_DIR/results/sf_1"
    exit 1
fi

DUCKDB_CSV="$BENCHMARK_DIR/results/duckdb_results.csv"
if [ ! -f "$DUCKDB_CSV" ]; then
    echo "Error: Baseline results $DUCKDB_CSV not found"
    exit 1
fi

# Collect benchmark results
COMBINED_CSV="$BENCHMARK_DIR/results/benchmark_results.csv"
echo "Query,Iteration,Engine,Time" > "$COMBINED_CSV"
tail -n +2 "$VELODB_CSV" | awk -F',' '{print $1","$3",VelODB,"$4/1000}' >> "$COMBINED_CSV"
tail -n +2 "$DUCKDB_CSV" | awk -F',' '{print $1","$2",DuckDB,"($3=="TIMEOUT"?$3:$3/1000)}' >> "$COMBINED_CSV"

# Plot benchmark results
source "$SCRIPTS_DIR/ensure_conda_env.sh"
FIGURE_DIR="$BASE_DIR/figures"
mkdir -p "$FIGURE_DIR"
BENCHMARK_PLOT="$FIGURE_DIR/benchmark_duckdb.pdf"
python3 "$SCRIPTS_DIR/plot_bars.py" \
    -i "$COMBINED_CSV" \
    -o "$BENCHMARK_PLOT" \
    --ylabel "Time (s)" \
    --bar-labels \
    --rotate-labels \
    --figsize 6,4 \
    --timeout-label ">60" \
    --ylim-scale 1.2
