# VelODB: Fast Oblivious Relational Database Queries within Confidential GPU

VelODB is a GPU-accelerated oblivious relational database system designed to execute SQL queries securely within confidential GPU environments. This artifact allows reviewers to:

1. Build VelODB from source
2. Run TPC-H benchmark queries (Q1, Q3, Q4, Q5, Q6, Q10, Q12, Q14, Q15, Q17, Q18, Q19)
3. Reproduce the performance results presented in the paper

---

## 1. Hardware Requirements

- GPU: NVIDIA GPU (Hopper or newer) with support for confidential computing
- Host CVM: Intel TDX or AMD SEV-SNP enabled system
    - RAM: Minimum 192GB (for scale factor 50)

---

## 2. Software Requirements

- **Operating System:** Ubuntu 20.04+ or equivalent Linux distribution
- **CUDA Toolkit:** 11.0 or newer
- **Compiler:** GCC 10+ with C++20 support
- **CMake:** 3.15 or newer
- **Python:** 3.12+ (for baseline comparisons and plotting)

### System Dependencies

```bash
sudo apt update
sudo apt install cmake ninja-build libgtest-dev bison flex elfutils libdw-dev git
```

### Python Dependencies (for baselines and plotting, conda recommended)

```bash
conda create -n velodb python=3.12
conda activate velodb
pip install seaborn pandas==2.3.3 duckdb
```

---

## 3. Getting Started

### 3.1 Clone the Repository

```bash
git clone --recursive https://github.com/macromogic/velodb.git
cd velodb
```

If you already cloned without `--recursive`:
```bash
git submodule update --init --recursive
```

### 3.2 Build VelODB

```bash
# Configure and build (Release mode)
cmake --preset release
cmake --build --preset release
```

For debug builds:
```bash
cmake --preset debug
cmake --build --preset debug
```

### 3.3 Verify Installation

```bash
# Run unit tests
ctest --preset release
```

---

## 4. Running TPC-H Benchmarks

### 4.1 Run Benchmarks (data will be generated automatically)

**Quick Test:**
```bash
scripts/run_benchmark.sh -s 0.01 -i 1 -q 1,6
```

**Full Benchmark (as in paper):**
```bash
scripts/run_benchmark.sh -s 0.1,1,2,3,5,10,50
```

### 4.2 Benchmark Options

| Option | Description | Default |
|--------|-------------|---------|
| `-s, --scale-factors` | Comma-separated scale factors | 0.01,0.1,1 |
| `-i, --iterations` | Number of iterations | 3 |
| `-q, --queries` | TPC-H query numbers | All supported |
| `-p, --with-profiling` | Enable detailed profiling | false |
| `-d, --benchmark-dir` | Output directory | benchmark_tpch |

---

## 5. Project Structure

```
velodb/
├── include/          # Header files
├── src/              # Source code
│   ├── cuda/         # CUDA kernels
│   ├── operator/     # Query operators
│   ├── planner/      # Query planner
│   └── execution/    # Execution engine
├── benchmark/        # TPC-H benchmark suite
│   ├── tpch/         # TPC-H queries and harness
│   └── data/         # Sample data files
├── scripts/          # Benchmark and plotting scripts
├── tests/            # Unit tests
└── thirdparty/       # Third-party dependencies
```

---

## 6. Reproducing Figures

Run all commands from the repository root with the `velodb` conda environment active. The shell plotting wrappers require conda, even if Python packages are installed elsewhere.

```bash
conda activate velodb
mkdir -p figures
```

The commands below are reconstructed from shell history and checked against the current scripts. Plotting wrappers select the newest CSV in `benchmark_tpch/results/sf_<SF>/`; use complete, non-profiled benchmark runs for performance comparisons. Figures 5 and 8 also accept `VELODB_CSV=/absolute/path/to/results.csv` to select a specific run. Profiling writes CSVs to the same directory, so generate these plots before running Figure 7, or explicitly select the non-profiled CSV.

For evaluation, retain the raw CSVs, profiling logs, repository revision (`git rev-parse HEAD`), local changes (`git diff`), GPU/driver information (`nvidia-smi`), compiler/toolkit versions, and Python package versions (`python3 -m pip freeze`) alongside the figures.

### Figure 2: The working GPU memory needed for SJ

``` bash
python3 scripts/memory_savings_analysis.py
```

Output: `figures/memory_savings_materialized_sf1000.pdf`

### Figure 3: The number of input/output rows for SJ

```bash
python3 scripts/row_reduction_analysis.py
```

Output: `figures/row_reduction_sf1000.pdf`

### Figure 5: Performance comparison with Opaque and Obliviator on TPC-H with scale factor 50


```bash
scripts/run_benchmark.sh -s 50 -i 3 --skip-baseline
SF=50 scripts/plot_obliv_baselines.sh
```

Output: `figures/benchmark_obliv_sf50.pdf`

This compares Q3, Q5, and Q6 using newly measured VelODB times and the checked-in `scripts/obliviator_sf50.csv`. Refer the scripts under `obliviator/tpch_*` to reproduce the results for Obliviator and Opaque (SGX SDK and Docker required).

### Figure 6: Comparison of the slowdown against non-oblivious variants on TPC-H with scale factor 50

```bash
python3 scripts/plot_slowdown.py \
    -i scripts/nobl_comparison.csv \
    -o figures/bench_obl_slowdown.pdf \
    --bar-labels --figsize 4,4 --ylim-scale 1.1
```

Output: `figures/bench_obl_slowdown.pdf`

### Figure 7: Execution time of VelODB compared to a non-oblivious variant on TPC-H with scale factor 50

```bash
scripts/run_benchmark.sh -s 50 -i 3 --skip-benchmark --skip-baseline -p

python3 scripts/generate_table.py \
    -i benchmark_tpch/results/profile_breakdown.log > obv_cost.csv

python3 scripts/plot_bars.py \
    -i obv_cost.csv -o figures/bench_olm_rev.pdf \
    --ylabel "Time (s)" --bar-labels --rotate-labels \
    --figsize 6,3.5 --ylim-scale 1.1 \
    --base-hue 'VelODB (non-oblivious)'
```

Output: `figures/bench_olm_rev.pdf`

### Figure 8: Execution time of VelODB vs. DuckDB on TPC-H with SF 50

```bash
scripts/run_benchmark.sh -s 50 -i 3 # add --skip-benchmark to run the baselines only
SF=50 scripts/plot_duckdb_baseline.sh
```

Output: `figures/benchmark_duckdb_sf50.pdf`

### Figure 9: Scalability of representative queries with increasing dataset sizes (scale factors 1--10)

```bash
scripts/run_benchmark.sh -s 1,2,3,5,10 -i 3 --skip-baseline
scripts/plot_scales.sh
```

Outputs: `figures/benchmark_scales_binary_join.pdf` (Q4, Q12, Q15) and `figures/benchmark_scales_multi_join.pdf` (Q5, Q10, Q18).
