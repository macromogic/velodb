# GPU Native TPC-H Benchmark

这是一个使用 **monolithic kernel** 设计的 GPU 原生 TPC-H benchmark 实现。

## 设计特点

### Monolithic Kernel 架构
- 每个 query 是一个单独的 CUDA kernel
- 使用 `cooperative_groups::grid_group::sync()` 实现 grid-wide 同步
- 通过 `cudaLaunchCooperativeKernel` 启动，与 VeloDB 保持一致

### 支持的 Queries
| Query | 描述 | Phases |
|-------|------|--------|
| Q01 | Simple filter on lineitem | 1 |
| Q03 | 3-way join: customer-orders-lineitem | 3 |
| Q04 | Semi-join: orders EXISTS lineitem | 2 |
| Q05 | 6-way join with region filter | 5 |
| Q06 | Multi-predicate filter | 1 |
| Q10 | 4-way join with return flag filter | 3 |
| Q12 | 2-way join with shipmode filter | 2 |
| Q14 | Lineitem-part join | 2 |
| Q15 | Supplier-lineitem join | 2 |
| Q17 | Lineitem-part join with filter | 2 |
| Q18 | 3-way join | 3 |
| Q19 | Complex multi-predicate join | 2 |

### 数据结构
- **列式存储**: 所有表使用列式布局，适合 GPU 向量化访问
- **Hash Table**: 使用 chained hash table 实现 join
- **String 编码**: 所有字符串类型预编码为整数

## 编译

```bash
cd benchmark/gpu_native
mkdir build && cd build
cmake ..
make -j
```

## 运行

```bash
# 运行所有 queries
./gpu_native_benchmark -d ../data

# 运行指定 query
./gpu_native_benchmark -d ../data -q q03

# 设置 warmup 和 benchmark 次数
./gpu_native_benchmark -d ../data -w 3 -r 10
```

## 参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `-d, --data-dir` | `../data` | TPC-H 数据目录 |
| `-q, --query` | `all` | 要运行的 query (q01-q19 或 all) |
| `-w, --warmup` | `2` | Warmup 次数 |
| `-r, --runs` | `5` | Benchmark 运行次数 |

## 文件结构

```
gpu_native/
├── CMakeLists.txt
├── main.cu                    # 主入口
├── common/
│   ├── types.hpp              # 数据类型定义
│   ├── hash_table.cuh         # Hash table 实现
│   ├── launcher.cuh           # Cooperative kernel 启动器
│   └── data_loader.cuh        # TBL 文件加载器
└── queries/
    ├── q01.cuh
    ├── q03.cuh
    ├── q04.cuh
    ├── q05.cuh
    ├── q06.cuh
    ├── q10.cuh
    ├── q12.cuh
    ├── q14.cuh
    ├── q15.cuh
    ├── q17.cuh
    ├── q18.cuh
    └── q19.cuh
```

## Kernel 执行模式

每个 monolithic kernel 遵循以下模式:

```cuda
__global__ void query_kernel(...) {
    cg::grid_group grid = cg::this_grid();

    // Phase 1: Build hash table / Filter
    for (i = grid.thread_rank(); i < n; i += grid.size()) {
        // ...
    }

    grid.sync();  // Grid-wide barrier

    // Phase 2: Probe / Join
    for (i = grid.thread_rank(); i < m; i += grid.size()) {
        // ...
    }

    grid.sync();  // Grid-wide barrier

    // Phase 3: Final output
    // ...
}
```

## 与 VeloDB 的对比

| 方面 | VeloDB | GPU Native |
|------|--------|------------|
| Kernel 类型 | Persistent + Command Queue | Per-query monolithic |
| 同步方式 | Command queue polling | grid.sync() |
| 调度 | Host-driven | Single kernel |
| 灵活性 | 高 (通用 operator) | 低 (硬编码) |
| 开销 | Command dispatch | 无 |
