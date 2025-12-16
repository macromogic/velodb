# VelODB TPC-H Benchmark Suite

This directory contains the TPC-H (Transaction Processing Performance Council - Ad Hoc) benchmark implementation for VelODB. TPC-H is a decision support benchmark that consists of a suite of business-oriented ad-hoc queries and concurrent data modifications.

## Overview

The benchmark suite provides:
- **Complete TPC-H Schema Implementation**: All 8 standard tables with proper VelODB type mapping
- **Query Templates**: Standard TPC-H queries (Q1, Q3, Q6, Q12 initially implemented)
- **Performance Monitoring**: Comprehensive metrics collection including memory usage, execution time, and GPU acceleration tracking
- **Data Loading Framework**: Efficient loading of TPC-H datasets with validation
- **Automated Reporting**: JSON, CSV, and text-based result export

## Directory Structure

```
benchmark/
├── infrastructure/           # Shared benchmark infrastructure
│   └── performance_monitor.* # Performance metrics collection
├── tpch/                    # TPC-H specific implementation
│   ├── data/
│   │   ├── schemas/        # TPC-H table schema definitions
│   │   └── loaders/        # Data loading utilities
│   ├── queries/
│   │   └── templates/      # SQL query templates
│   ├── harness/            # Benchmark execution framework
│   └── main.cpp            # Benchmark executable
├── tests/                  # Unit tests for benchmark components
└── results/                # Benchmark results output (created at runtime)
```

## Quick Start

### 1. Build the Benchmark

```bash
cd build
ninja tpch_benchmark
```

### 2. Prepare TPC-H Data

Generate TPC-H data files using the official `dbgen` tool or download pre-generated data:

```bash
# Example for scale factor 0.01 (10MB dataset)
mkdir -p benchmark/data
# Place .tbl files (customer.tbl, orders.tbl, lineitem.tbl, etc.) in benchmark/data/
```

### 3. Run Quick Benchmark

```bash
# Quick test with small dataset
./bin/tpch_benchmark --quick

# Or custom configuration
./bin/tpch_benchmark --scale-factor 0.01 --queries 1,6 --iterations 1 --verbose
```

### 4. Run Standard Benchmark

```bash
# Standard benchmark configuration
./bin/tpch_benchmark --standard

# Or full custom benchmark
./bin/tpch_benchmark --scale-factor 0.1,1.0 --queries 1,3,6,12 --iterations 3
```

## Command Line Options

| Option | Description | Default |
|--------|-------------|---------|
| `--scale-factor`, `-s` | Scale factors to test (comma-separated) | 0.01 |
| `--queries`, `-q` | Query numbers to run (comma-separated) | 1,6 |
| `--iterations`, `-i` | Number of iterations per query | 1 |
| `--data-dir`, `-d` | Directory containing TPC-H data files | ./benchmark/data |
| `--results-dir`, `-r` | Directory to store results | ./benchmark/results |
| `--test-type`, `-t` | Test type: power, throughput, full | power |
| `--verbose`, `-v` | Enable verbose output | false |
| `--export-format` | Export format: json, csv | json |
| `--quick` | Run quick benchmark preset | false |
| `--standard` | Run standard benchmark preset | false |

## Benchmark Presets

### Quick Benchmark
- Scale Factor: 0.01 (10MB)
- Queries: Q1, Q6
- Iterations: 1
- Purpose: Development and CI testing

### Standard Benchmark
- Scale Factor: 0.1 (100MB)
- Queries: Q1, Q3, Q6, Q12
- Iterations: 3
- Purpose: Performance evaluation

## Implemented Queries

| Query | Name | Description | Status |
|-------|------|-------------|--------|
| Q1 | Pricing Summary Report | Aggregation with filtering | ✅ Implemented |
| Q3 | Shipping Priority | Multi-table joins | ✅ Implemented |
| Q6 | Forecasting Revenue Change | Simple aggregation with filters | ✅ Implemented |
| Q12 | Shipping Modes and Order Priority | Complex joins and case expressions | ✅ Implemented |

Additional queries will be implemented in future iterations.

## Performance Metrics

The benchmark collects comprehensive performance metrics:

### Query-Level Metrics
- **Execution Time**: Total query execution time
- **Planning Time**: Query planning and optimization time
- **Memory Usage**: Peak memory consumption during execution
- **GPU Memory**: GPU memory utilization (if CUDA enabled)
- **Rows Processed**: Number of rows processed
- **Late Materialization**: Whether late materialization was used

### System-Level Metrics
- CPU information and specifications
- GPU information (if available)
- Total and available system memory
- Timestamp and benchmark configuration

## Results and Reporting

### Result Files
Results are automatically exported to the `results/` directory:

- `tpch_benchmark_YYYYMMDD_HHMMSS.json` - Detailed results in JSON format
- `tpch_summary_YYYYMMDD_HHMMSS.txt` - Human-readable summary report

### Sample Summary Report
```
VelODB TPC-H Benchmark Report
=============================

Benchmark ID: 20250911_143022
Timestamp: 2025-09-11 14:30:22
CPU: Intel(R) Core(TM) i9-9900K CPU @ 3.60GHz
GPU: NVIDIA GeForce RTX 3080 (Compute Capability 8.6)
Total Memory: 32768 MB

Configuration:
--------------
Scale Factors: 0.01
Queries: Q1, Q6
Iterations: 1
Data Load Time: 2.341s
Total Benchmark Time: 3.127s

Query Results:
--------------
Query    SF Iter    Time(s)     Rows   Memory   Status     LM
-------------------------------------------------------------
   Q1  0.01    1      0.087     4000        8       OK      Y
   Q6  0.01    1      0.023        1        4       OK      Y

Summary:
--------
Total Queries: 2
Successful: 2 (100.0%)
Average Query Time: 0.055s
Late Materialization Usage: 2/2 (100.0%)
```

## Data Requirements

### TPC-H Data Files
The benchmark expects standard TPC-H `.tbl` files in pipe-delimited format:

```
benchmark/data/
├── customer.tbl
├── lineitem.tbl
├── nation.tbl
├── orders.tbl
├── part.tbl
├── partsupp.tbl
├── region.tbl
└── supplier.tbl
```

### Scale Factors
Supported scale factors and approximate dataset sizes:

| Scale Factor | Dataset Size | Customer Rows | Lineitem Rows | Use Case |
|--------------|--------------|---------------|---------------|----------|
| 0.01 | ~10MB | 1,500 | 60,000 | Development/Testing |
| 0.1 | ~100MB | 15,000 | 600,000 | Integration Testing |
| 1.0 | ~1GB | 150,000 | 6,000,000 | Standard Benchmark |
| 10.0 | ~10GB | 1,500,000 | 60,000,000 | Large-scale Testing |

## Integration with VelODB Features

### Late Materialization Testing
The benchmark specifically tests VelODB's late materialization optimization:
- Tracks whether late materialization was used for each query
- Measures performance improvement from delayed tuple construction
- Validates column-oriented storage benefits

### GPU Acceleration
When CUDA is enabled, the benchmark:
- Monitors GPU memory usage during query execution
- Tracks GPU acceleration effectiveness
- Measures data transfer overhead

### Query Optimization
The benchmark validates VelODB's query optimization capabilities:
- Cost-based query planning
- Join order optimization
- Predicate pushdown effectiveness

## Development and Testing

### Running Tests
```bash
# Run benchmark-specific tests
ninja benchmark_tests
./bin/benchmark_tests

# Run specific test suites
./bin/benchmark_tests --gtest_filter="TPCHSchemas*"
```

### Adding New Queries
1. Create SQL template in `queries/templates/qXX.sql`
2. Add parameter substitution in `QueryParameterGenerator`
3. Update `BenchmarkConfig::query_numbers` defaults
4. Add query-specific validation logic

### Custom Scale Factors
To add support for custom scale factors:
1. Update `ScaleConfigurations::scale_configs_`
2. Generate appropriate TPC-H data using `dbgen`
3. Update validation tolerances in `TPCHValidator`

## Troubleshooting

### Common Issues

**Data files not found:**
```
Error: TPC-H data files not found for SF=0.01 in directory: ./benchmark/data
```
- Ensure `.tbl` files are present in the data directory
- Check file permissions and naming conventions

**Memory allocation errors:**
```
Error: Failed to allocate memory for table loading
```
- Reduce batch size in `LoadConfig`
- Use smaller scale factor for testing
- Check available system memory

**Query execution failures:**
```
Query Q3 failed: Table 'customer' not found
```
- Verify all required tables are loaded
- Check table schema compatibility
- Ensure database initialization completed

### Performance Tuning

For optimal benchmark performance:
1. Use SSD storage for data files
2. Ensure sufficient RAM (2x dataset size recommended)
3. Enable GPU acceleration if available
4. Use appropriate batch sizes for data loading
5. Run multiple iterations for stable measurements

## Contributing

When contributing to the benchmark suite:
1. Follow VelODB coding standards and patterns
2. Add comprehensive tests for new features
3. Update documentation for new queries or features
4. Validate results against reference TPC-H implementations
5. Ensure backward compatibility with existing configurations

## References

- [TPC-H Benchmark Specification](http://www.tpc.org/tpch/)
- [VelODB Architecture Documentation](../docs/)
- [CUDA Programming Guide](https://docs.nvidia.com/cuda/)
