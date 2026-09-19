#!/bin/bash -eu

SCRIPTS_DIR="$(realpath "$(dirname "$0")")"
BASE_DIR="$(realpath "$(dirname "$0")/..")"
BENCHMARK_DIR="$BASE_DIR/benchmark_tpch"
SF="${SF:-1}"
LATEST_VELODB_CSV=$(ls -t "$BENCHMARK_DIR/results/sf_$SF"/*.csv 2>/dev/null | head -1)

VELODB_CSV="${VELODB_CSV:-$LATEST_VELODB_CSV}"
if [ ! -n "$VELODB_CSV" ]; then
    echo "Error: No benchmark results found in $BENCHMARK_DIR/results/sf_$SF"
    exit 1
fi

DUCKDB_CSV="$BENCHMARK_DIR/results/duckdb_results_sf${SF}.csv"
if [ ! -f "$DUCKDB_CSV" ]; then
    echo "Error: Baseline results $DUCKDB_CSV not found"
    exit 1
fi

DUCKDB16_CSV="$BENCHMARK_DIR/results/duckdb16_results_sf${SF}.csv"
if [ ! -f "$DUCKDB16_CSV" ]; then
    echo "Error: Baseline results $DUCKDB16_CSV not found"
    exit 1
fi

# Collect benchmark results
COMBINED_CSV="$BENCHMARK_DIR/results/benchmark_results.csv"
echo "Query,Iteration,Engine,Time" > "$COMBINED_CSV"
tail -n +2 "$VELODB_CSV" | awk -F',' '{print $1","$3",VelODB,"$4/1000}' >> "$COMBINED_CSV"
tail -n +2 "$DUCKDB_CSV" | awk -F',' '{print $1","$2",DuckDB,"($3=="TIMEOUT"?$3:$3/1000)}' >> "$COMBINED_CSV"
tail -n +2 "$DUCKDB16_CSV" | awk -F',' '{print $1","$2",DuckDB16,"($3=="TIMEOUT"?$3:$3/1000)}' >> "$COMBINED_CSV"

# Plot benchmark results
source "$SCRIPTS_DIR/ensure_conda_env.sh"
FIGURE_DIR="$BASE_DIR/figures"
mkdir -p "$FIGURE_DIR"
BENCHMARK_PLOT="$FIGURE_DIR/benchmark_duckdb_sf${SF}.pdf"
TIMEOUT=$((SF * 60))
python3 "$SCRIPTS_DIR/plot_bars.py" \
    -i "$COMBINED_CSV" \
    -o "$BENCHMARK_PLOT" \
    --ylabel "Time (s)" \
    --bar-labels \
    --rotate-labels \
    --figsize 6,3.5 \
    --timeout-label ">${TIMEOUT}" \
    --ylim-scale 1.2
