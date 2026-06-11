#!/bin/bash -eu

SCRIPTS_DIR="$(realpath "$(dirname "$0")")"
BASE_DIR="$(realpath "$(dirname "$0")/..")"
BENCHMARK_DIR="$BASE_DIR/benchmark_tpch"
LATEST_VELODB_CSV=$(ls -t "$BENCHMARK_DIR/results/sf_1"/*.csv 2>/dev/null | head -1)

VELODB_CSV="${VELODB_CSV:-$LATEST_VELODB_CSV}"
if [ ! -n "$VELODB_CSV" ]; then
    echo "Error: No benchmark results found in $BENCHMARK_DIR/results/sf_1"
    exit 1
fi

# The baseline data is collected from replicating the experiments
# in the Obliviator paper, excluding aggregation stages and using
# the same machine as for VelODB benchmarks.
BASELINE_CSV="$SCRIPTS_DIR/obliviator_sf50.csv"

COMBINED_CSV="$BENCHMARK_DIR/results/sf50_benchmark_comparison.csv"
echo "Query,Iteration,Engine,Time" > "$COMBINED_CSV"
tail -n +2 "$BASELINE_CSV" | awk -F',' '
{
    if ($1 == "OBL") {
        print $2 ",1,Obliviator," $3
    } else if ($1 == "OPQ") {
        print $2 ",1,Opaque," $3
    }
}
' >> "$COMBINED_CSV"

tail -n +2 "$VELODB_CSV" | awk -F',' '
$1 == "Q3" || $1 == "Q5" || $1 == "Q6" {
    print $1 "," $3 ",VelODB," $4 / 1000
}
' >> "$COMBINED_CSV"

# Plot benchmark results
source "$SCRIPTS_DIR/ensure_conda_env.sh"
FIGURE_DIR="$BASE_DIR/figures"
mkdir -p "$FIGURE_DIR"
BENCHMARK_PLOT="$FIGURE_DIR/benchmark_obliv.pdf"
python3 "$SCRIPTS_DIR/plot_bars.py" \
    -i "$COMBINED_CSV" \
    -o "$BENCHMARK_PLOT" \
    --log-scale \
    --ylabel "Time (s)" \
    --figsize 6,3 \
    --base-hue "VelODB" \
    --bar-labels \
    --ylim-scale 1.1 \
