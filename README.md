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
git clone --recursive [this-repo-url]
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
cd build
ctest --output-on-failure
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
