# 硬件旋钮测试项目

本项目基于 `/home/steve/yunwei37/ebpf-cpu/docs/idea.md` 文档，实现了各种硬件旋钮的测试程序。

## 硬件旋钮概述

现代处理器提供了多种硬件旋钮，允许软件实时调整硬件行为以优化性能和能耗。这些旋钮可以通过 MSR (Model Specific Registers)、系统调用或特殊接口进行控制。

### 核心硬件旋钮类型

#### 1. RDT (Resource Director Technology) - 资源分配控制

**原理**: Intel RDT 技术允许监控和控制处理器资源的使用，包括 Last Level Cache (LLC) 和内存带宽。

**关键寄存器**:
- `IA32_L3_MASK_N` (MSR 0xC90+N): 控制 LLC 分配 mask
- `IA32_PQR_ASSOC` (MSR 0xC8F): 设置当前线程的 CLOS ID
- `IA32_QM_EVTSEL` (MSR 0xC8D): 选择监控事件类型

**控制机制**:
- 通过 `/sys/fs/resctrl/` 接口或直接写 MSR
- 可以按 CLOS (Class of Service) 分配 LLC 片区和内存带宽
- 支持实时监控内存带宽使用情况

**性能影响**: 对于在线服务 + 批处理混合负载，可实现 P99 延迟降低 35%，IOPS 提升 57%

#### 2. Hardware Prefetcher - 硬件预取器控制

**原理**: 现代处理器包含多级硬件预取器，可以预测内存访问模式并提前加载数据。

**关键寄存器**:
- `MSR_MISC_FEATURE_CONTROL` (MSR 0x1A4): Intel 预取器控制
- `MSR_PREFETCH_CONTROL` (MSR 0x1A0): 预取器开关控制

**控制机制**:
- 可以独立开关不同级别的预取器
- 支持基于负载特征的自适应预取策略
- 根据 LLC miss 率动态调整预取强度

**性能影响**: 对于图计算和流式计算负载，可实现吞吐量提升 8-20%，同时 DDR 能耗降低 10%

#### 3. SMT (Simultaneous Multi-Threading) - 超线程控制

**原理**: SMT 技术允许单个物理核心同时执行多个线程，但在某些情况下关闭 SMT 可以提升单线程性能。

**控制机制**:
- 全局控制: `/sys/devices/system/cpu/smt/control`
- 单核控制: `/sys/devices/system/cpu/cpuN/online`
- 动态控制: 基于 IPC 和内存停顿检测

**性能影响**: 对于延迟敏感应用，可实现 P99 延迟降低 25%

#### 4. Uncore Frequency - 非核心频率控制

**原理**: Uncore 包括 LLC、内存控制器、QPI/UPI 等组件，其频率可以独立于核心频率进行调整。

**关键寄存器**:
- `MSR_UNCORE_RATIO_LIMIT` (MSR 0x620): 设置 Uncore 频率限制
- `MSR_UNCORE_PERF_STATUS` (MSR 0x621): 读取当前 Uncore 状态

**控制机制**:
- 通过 `intel_uncore_frequency` 驱动
- 基于内存带宽需求动态调频
- 结合 RDT 监控数据进行智能调频

**性能影响**: 对于 OLTP + Analytics 混合负载，可实现每操作能耗降低 10-15%

#### 5. RAPL (Running Average Power Limit) - 功耗控制

**原理**: RAPL 技术允许设置和监控处理器功耗限制，支持包级和核心级功耗控制。

**关键寄存器**:
- `MSR_PKG_POWER_LIMIT` (MSR 0x610): 包级功耗限制
- `MSR_PP0_POWER_LIMIT` (MSR 0x638): 核心功耗限制
- `MSR_PKG_ENERGY_STATUS` (MSR 0x611): 包级能耗计数

**控制机制**:
- 通过 `/sys/class/powercap/intel-rapl/` 接口
- 支持短期和长期功耗限制
- 可以设置功耗监控窗口

**性能影响**: 在数据中心环境中可改善 PUE，同时保持性能稳定

#### 6. CXL (Compute Express Link) - 内存扩展控制

**原理**: CXL 技术提供高带宽、低延迟的内存扩展能力，支持动态内存管理。

**控制机制**:
- HDM Decoder 重新映射：动态调整内存拓扑
- 带宽节流：Performance Throttle Control
- 内存侧缓存：MSC Flush/Bypass 控制

**性能影响**: 对于大内存应用，可实现尾延迟降低 25%，带宽利用率提升 24%

## 项目结构

```
hardware-knobs/
├── README.md               # 本文档
├── Makefile               # 编译所有测试程序
├── common/                # 通用工具和头文件
│   ├── msr_utils.h        # MSR 操作工具
│   ├── msr_utils.c        # MSR 操作实现
│   └── common.h           # 通用定义
├── rdt/                   # RDT 相关测试
│   ├── rdt_test.c         # RDT 功能测试
│   └── rdt_monitor.c      # RDT 监控测试
├── prefetch/              # 预取器相关测试
│   ├── prefetch_test.c    # 预取器控制测试
│   └── prefetch_bench.c   # 预取器性能测试
├── smt/                   # SMT 相关测试
│   ├── smt_test.c         # SMT 控制测试
│   └── smt_bench.c        # SMT 性能测试
├── uncore/                # Uncore 相关测试
│   ├── uncore_test.c      # Uncore 频率测试
│   └── uncore_monitor.c   # Uncore 监控测试
├── rapl/                  # RAPL 相关测试
│   ├── rapl_test.c        # RAPL 功耗测试
│   └── rapl_monitor.c     # RAPL 监控测试
└── cxl/                   # CXL 相关测试
    ├── cxl_test.c         # CXL 功能测试
    └── cxl_monitor.c      # CXL 监控测试
```

## 编译和运行

```bash
# 编译所有测试程序
make all

# 运行特定测试
make test-rdt
make test-prefetch
make test-smt
make test-uncore
make test-rapl
make test-cxl

# 清理编译结果
make clean
```

## 使用注意事项

1. **权限要求**: 大部分测试需要 root 权限或 CAP_SYS_ADMIN 能力
2. **硬件支持**: 确保硬件平台支持相应的特性
3. **内核支持**: 需要相应的内核驱动和配置
4. **安全考虑**: 直接操作 MSR 需要谨慎，错误的设置可能导致系统不稳定

## 理论支撑

### 行为-驱动硬件自适应闭环

本项目实现了在软件关键行为节点触发时，用程序将"监测事件"立即映射到"硬件旋钮"，形成微秒级闭环：

1. **行为节点**: 调度器切换、内存缺页、网络中断等
2. **监测机制**: PMU 计数器、温度传感器、内存带宽监控
3. **决策逻辑**: 基于阈值或机器学习的策略
4. **执行机制**: 直接写 MSR 或通过内核接口

### 性能收益模型

根据文档中的数据，不同负载类型的性能收益：

- **在线服务 + 批处理**: P99 延迟降低 35-60%
- **AI CPU 推理**: 帧率提升 15-25%
- **内存带宽瓶颈 HPC**: 吞吐量提升 30-40%
- **函数计算 FaaS**: P99 延迟降低 20-40%

## 参考文献

1. Intel® 64 and IA-32 Architectures Software Developer's Manual
2. Intel® Resource Director Technology (RDT) Architecture Specification
3. CXL™ Specification
4. dCat: Dynamic Cache Management for Efficient, Performance-sensitive Infrastructure-as-a-Service
5. Intel® Hardware Prefetch Control for Intel® Atom™ Cores