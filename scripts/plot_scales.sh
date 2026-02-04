#!/bin/bash -eu

SCRIPTS_DIR="$(realpath "$(dirname "$0")")"
BASE_DIR="$(realpath "$(dirname "$0")/..")"
BENCHMARK_DIR="$BASE_DIR/benchmark_tpch"

COMBINED_CSV="$BENCHMARK_DIR/results/scale_results.csv"
echo "Query,Iteration,SF,Time" > "$COMBINED_CSV"
for SF in 1 2 3 5 10; do
    VELODB_CSV=$(ls -t "$BENCHMARK_DIR/results/sf_$SF"/*.csv 2>/dev/null | head -1)
    if [ ! -n "$VELODB_CSV" ]; then
        echo "Error: No benchmark results found in $BENCHMARK_DIR/results/sf_$SF"
        exit 1
    fi
    tail -n +2 "$VELODB_CSV" | awk -F',' -v SF="$SF" '{print $1","$3","SF","$4/1000}' >> "$COMBINED_CSV"
done

# Plot benchmark results
source "$SCRIPTS_DIR/ensure_conda_env.sh"
FIGURE_DIR="$BASE_DIR/figures"
mkdir -p "$FIGURE_DIR"
python3 "$SCRIPTS_DIR/plot_lines.py" \
    -i "$COMBINED_CSV" \
    -o "$FIGURE_DIR/benchmark_scales_binary_join.pdf" \
    --ylabel "Time (s)" \
    --hue "Query" \
    --figsize 4,4 \
    --queries "Q4,Q12,Q15"
python3 "$SCRIPTS_DIR/plot_lines.py" \
    -i "$COMBINED_CSV" \
    -o "$FIGURE_DIR/benchmark_scales_multi_join.pdf" \
    --ylabel "Time (s)" \
    --hue "Query" \
    --figsize 4,4 \
    --queries "Q5,Q10,Q18"
