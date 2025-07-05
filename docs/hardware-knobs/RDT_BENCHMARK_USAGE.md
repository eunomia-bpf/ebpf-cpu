# RDT Benchmark Usage Guide

## 概述

RDT (Resource Director Technology) 基准测试套件是一个全面的性能测试工具，用于演示和测量 Intel RDT 技术的各种效果。该工具包含多种工作负载类型和配置，可以帮助用户理解 RDT 对不同应用场景的性能影响。

## 快速开始

### 1. 编译基准测试

```bash
# 编译所有 RDT 相关程序
make rdt

# 或者编译整个项目
make all
```

### 2. 运行基准测试

```bash
# 运行所有配置的基准测试（需要 root 权限）
sudo ./build/rdt_bench

# 运行特定配置的基准测试
sudo ./build/rdt_bench [配置编号]

# 使用 Makefile 快捷方式
sudo make bench-rdt
```

## 基准测试配置

### 配置列表

| 编号 | 配置名称 | L3 缓存掩码 | 内存带宽限制 | 线程数 | 工作负载类型 |
|------|----------|-------------|--------------|--------|-------------|
| 0    | Baseline - No RDT Control | 0xFFFF (100%) | 0% | 4 | Cache Intensive |
| 1    | Cache Isolation - High Priority | 0xFF00 (50%) | 0% | 2 | Cache Intensive |
| 2    | Cache Isolation - Low Priority | 0x00FF (50%) | 0% | 2 | Cache Intensive |
| 3    | Memory Bandwidth Throttling - 50% | 0xFFFF (100%) | 50% | 4 | Memory Intensive |
| 4    | Memory Bandwidth Throttling - 25% | 0xFFFF (100%) | 25% | 4 | Memory Intensive |
| 5    | Mixed Workload - Balanced | 0xFFFF (100%) | 0% | 8 | Mixed Workload |
| 6    | Pointer Chase - Cache Sensitive | 0xF000 (25%) | 0% | 2 | Pointer Chase |
| 7    | Stream Copy - Bandwidth Sensitive | 0xFFFF (100%) | 75% | 4 | Stream Copy |

### 工作负载类型说明

#### 1. Cache Intensive（缓存密集型）
- **特征**: 频繁访问小规模数据集，高缓存局部性
- **测试目标**: 评估缓存分配策略的效果
- **关键指标**: 缓存命中率、操作吞吐量

#### 2. Memory Intensive（内存密集型）
- **特征**: 顺序访问大规模数据集，产生大量内存流量
- **测试目标**: 评估内存带宽控制的效果
- **关键指标**: 内存带宽利用率、数据传输速率

#### 3. Mixed Workload（混合负载）
- **特征**: 结合缓存密集型和内存密集型操作
- **测试目标**: 评估复杂应用场景下的性能
- **关键指标**: 整体吞吐量、延迟分布

#### 4. Pointer Chase（指针追踪）
- **特征**: 随机内存访问模式，对缓存敏感
- **测试目标**: 评估缓存分配对随机访问的影响
- **关键指标**: 延迟、缓存失效率

#### 5. Stream Copy（流复制）
- **特征**: 大量连续内存复制操作
- **测试目标**: 评估内存带宽限制的效果
- **关键指标**: 内存带宽、传输效率

## 输出解读

### 基准测试结果

```
=== Benchmark Results: Baseline - No RDT Control ===
L3 Cache Mask: 0xFFFF
Memory Bandwidth Throttle: 0%
Number of Threads: 4
Benchmark Type: Cache Intensive

Per-Thread Results:
Thread  Throughput    Latency(ms)  Duration(s)
------  ----------    -----------  -----------
     0       64.61       30000.13        30.00
     1       64.69       30000.15        30.00
     2       68.18       30000.05        30.00
     3       70.48       30000.15        30.00
------  ----------    -----------  -----------
Total       267.96       30000.12
```

#### 关键指标说明

1. **Throughput（吞吐量）**: 
   - 缓存密集型：百万次操作/秒
   - 内存密集型：MB/s
   - 混合负载：百万次操作/秒

2. **Latency（延迟）**: 
   - 单位：毫秒
   - 表示完成所有操作所需的时间

3. **Duration（持续时间）**: 
   - 单位：秒
   - 基准测试的实际运行时间

### RDT 监控数据

```
Time(s)  LLC Occupancy(KB)  MBM Total(MB/s)  MBM Local(MB/s)
-------  -----------------  ---------------  ---------------
    0.0                  0                0                0
    1.0                  0                0                0
```

#### 监控指标说明

1. **LLC Occupancy**: Last Level Cache 占用量（KB）
2. **MBM Total**: 总内存带宽使用量（MB/s）
3. **MBM Local**: 本地内存带宽使用量（MB/s）

*注意：在不支持 RDT 监控的系统上，这些值可能显示为 0 或模拟数据。*

## 高级用法

### 1. 自定义配置

可以通过修改 `rdt_bench.c` 中的 `benchmark_configs` 数组来添加自定义配置：

```c
{
    .name = "自定义配置",
    .l3_mask = 0xFFF0,      // L3 缓存掩码
    .mb_throttle = 30,       // 内存带宽限制 (%)
    .num_threads = 6,        // 线程数量
    .bench_type = BENCH_MIXED_WORKLOAD  // 工作负载类型
}
```

### 2. 批量测试

创建脚本进行批量测试：

```bash
#!/bin/bash
# 批量运行所有配置
for i in {0..7}; do
    echo "=== Running configuration $i ==="
    sudo ./build/rdt_bench $i > results_$i.txt 2>&1
    sleep 5
done
```

### 3. 性能分析

结合其他工具进行深度分析：

```bash
# 使用 perf 监控缓存性能
sudo perf stat -e LLC-loads,LLC-load-misses,LLC-stores,LLC-store-misses ./build/rdt_bench 0

# 使用 Intel PCM 监控内存带宽
sudo pcm-memory.x 1 &
sudo ./build/rdt_bench 3
```

## 故障排除

### 常见问题

1. **权限错误**
   ```
   ERROR: Root privileges required for MSR access
   ```
   **解决方案**: 使用 `sudo` 运行命令

2. **MSR 模块未加载**
   ```
   ERROR: Failed to load MSR module
   ```
   **解决方案**: 
   ```bash
   sudo modprobe msr
   ```

3. **RDT 不支持**
   ```
   ERROR: RDT not supported on this CPU
   ```
   **解决方案**: 确认 CPU 支持 RDT 功能
   ```bash
   grep -i rdt /proc/cpuinfo
   ```

4. **MSR 访问错误**
   ```
   ERROR: Failed to write MSR 0xc8d: Input/output error
   ```
   **说明**: 这通常表示硬件不支持特定的 RDT 功能，但不影响基准测试的基本功能。

### 调试模式

编译调试版本：

```bash
make debug
sudo ./build/rdt_bench 0
```

## 性能调优建议

### 1. 系统准备

- 关闭不必要的后台进程
- 设置 CPU 频率为固定值
- 禁用 CPU 功耗管理特性

```bash
# 设置性能模式
sudo cpupower frequency-set -g performance

# 禁用 turbo boost
echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo
```

### 2. 测试环境

- 确保系统内存充足
- 避免在虚拟化环境中运行（可能影响 RDT 功能）
- 使用专用的测试环境

### 3. 结果分析

- 多次运行取平均值
- 关注相对性能变化而非绝对值
- 结合系统监控工具分析

## 扩展开发

### 添加新的工作负载类型

1. 在 `benchmark_type_t` 枚举中添加新类型
2. 实现对应的基准测试函数
3. 在 `benchmark_thread()` 函数中添加处理逻辑
4. 更新配置数组

### 集成到应用程序

可以将基准测试逻辑集成到自己的应用程序中：

```c
#include "rdt_bench.h"

// 在应用程序中运行特定工作负载
double result = benchmark_cache_intensive(data, size, &running);
```

## 参考资料

- [Intel RDT 官方文档](https://www.intel.com/content/www/us/en/architecture-and-technology/resource-director-technology.html)
- [Linux resctrl 文档](https://www.kernel.org/doc/Documentation/x86/resctrl.rst)
- [RDT 详细技术文档](./RDT详细文档.md)

## 联系与支持

如遇问题或需要技术支持，请：

1. 检查系统日志获取更多信息
2. 确认硬件和软件环境满足要求
3. 参考故障排除部分
4. 提交详细的错误报告