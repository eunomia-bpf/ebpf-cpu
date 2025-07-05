# 现代 CPU 可编程功能详解

## 目录

1. [概述](#概述)
2. [频率和电源管理](#频率和电源管理)
3. [缓存控制](#缓存控制)
4. [性能监控和计数器](#性能监控和计数器)
5. [安全特性](#安全特性)
6. [虚拟化支持](#虚拟化支持)
7. [SIMD 指令集](#simd-指令集)
8. [内存管理](#内存管理)
9. [调试和追踪](#调试和追踪)
10. [其他高级特性](#其他高级特性)

## 概述

现代 CPU 提供了大量可通过指令编程调整的功能，这些功能允许软件动态优化性能、功耗、安全性等方面。本文档详细介绍了这些功能及其编程接口。

## 频率和电源管理

### 1. 动态频率调节（DVFS）

**功能描述**：允许软件动态调整 CPU 的工作频率和电压。

**相关指令和接口**：
- MSR（Model Specific Register）访问：`RDMSR`/`WRMSR`
- Intel SpeedStep：通过 MSR `0x198`（IA32_PERF_STATUS）和 `0x199`（IA32_PERF_CTL）
- AMD Cool'n'Quiet：通过 MSR `0xC0010062`（P-State Control）

**编程示例**：
```c
// 读取当前 P-State
uint64_t read_pstate() {
    uint32_t low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(0x198));
    return ((uint64_t)high << 32) | low;
}
```

### 2. C-States（CPU 空闲状态）

**功能描述**：控制 CPU 在空闲时的功耗状态。

**相关指令**：
- `MONITOR`/`MWAIT`：进入特定的 C-state
- `HLT`：进入 C1 状态
- MSR `0xE2`（PKG_CST_CONFIG_CONTROL）：配置包级 C-state

### 3. Turbo Boost / Turbo Core

**功能描述**：自动提升 CPU 频率超过基础频率。

**控制方法**：
- MSR `0x1A0`（IA32_MISC_ENABLE）的第 38 位：启用/禁用 Turbo
- MSR `0x1AD`（MSR_TURBO_RATIO_LIMIT）：设置 Turbo 频率限制

## 缓存控制

### 1. 缓存预取控制

**功能描述**：控制硬件预取器的行为。

**相关指令**：
- `PREFETCH` 系列指令：`PREFETCHT0/T1/T2/NTA`
- MSR `0x1A4`（MSR_PREFETCH_CONTROL）：控制硬件预取器

**编程示例**：
```c
// 软件预取
void prefetch_data(void* addr) {
    asm volatile("prefetcht0 %0" : : "m"(*(char*)addr));
}
```

### 2. 缓存刷新和失效

**相关指令**：
- `CLFLUSH`：刷新特定缓存行
- `CLFLUSHOPT`：优化的缓存刷新
- `CLWB`：写回缓存行但保持有效
- `WBINVD`：写回并失效整个缓存

### 3. 缓存分配技术（CAT）

**功能描述**：Intel RDT（Resource Director Technology）的一部分，允许为不同的应用分配 LLC 缓存。

**控制方法**：
- MSR `0xC90`（IA32_L3_MASK_0）等：设置缓存掩码
- CPUID 叶 0x10：查询 CAT 能力

## 性能监控和计数器

### 1. 性能监控计数器（PMC）

**功能描述**：监控各种微架构事件。

**相关指令和寄存器**：
- `RDPMC`：读取性能计数器
- MSR `0x186-0x189`（IA32_PERFEVTSELx）：选择要监控的事件
- MSR `0xC1-0xC8`（IA32_PMCx）：性能计数器

**编程示例**：
```c
// 配置 PMC 监控 L3 缓存未命中
void setup_l3_miss_counter() {
    uint64_t evtsel = 0x412E; // L3 miss event
    evtsel |= (1 << 22);      // Enable counter
    evtsel |= (1 << 16);      // User mode
    wrmsr(0x186, evtsel);
}

// 读取计数器
uint64_t read_pmc(unsigned int counter) {
    uint32_t low, high;
    asm volatile("rdpmc" : "=a"(low), "=d"(high) : "c"(counter));
    return ((uint64_t)high << 32) | low;
}
```

### 2. 时间戳计数器（TSC）

**相关指令**：
- `RDTSC`：读取时间戳计数器
- `RDTSCP`：读取 TSC 和处理器 ID

## 安全特性

### 1. SMEP/SMAP（Supervisor Mode Execution/Access Prevention）

**功能描述**：防止内核执行/访问用户空间代码/数据。

**控制方法**：
- CR4 寄存器的第 20 位（SMEP）和第 21 位（SMAP）
- `STAC`/`CLAC`：临时启用/禁用 SMAP

### 2. CET（Control-flow Enforcement Technology）

**功能描述**：提供影子栈和间接分支跟踪。

**相关指令**：
- `ENDBR32/64`：合法的间接分支目标
- `WRSS`/`RDSS`：影子栈操作

### 3. SGX（Software Guard Extensions）

**功能描述**：创建安全飞地执行环境。

**相关指令**：
- `ENCLS`：特权模式 SGX 指令
- `ENCLU`：用户模式 SGX 指令

## 虚拟化支持

### 1. VMX（Intel）/ SVM（AMD）

**功能描述**：硬件辅助虚拟化。

**相关指令**：
- Intel VMX：`VMXON`、`VMLAUNCH`、`VMRESUME`、`VMEXIT`
- AMD SVM：`VMRUN`、`VMSAVE`、`VMLOAD`

### 2. EPT/NPT（扩展/嵌套页表）

**功能描述**：第二级地址转换。

**控制方法**：
- 通过 VMCS（Intel）或 VMCB（AMD）配置

## SIMD 指令集

### 1. AVX-512

**功能描述**：512 位向量运算。

**特殊功能**：
- 掩码寄存器：`K0-K7`
- 舍入控制：每条指令级别的舍入模式
- 异常抑制

**编程示例**：
```c
// AVX-512 向量加法
void vector_add_avx512(float* a, float* b, float* c, int n) {
    for (int i = 0; i < n; i += 16) {
        __m512 va = _mm512_load_ps(&a[i]);
        __m512 vb = _mm512_load_ps(&b[i]);
        __m512 vc = _mm512_add_ps(va, vb);
        _mm512_store_ps(&c[i], vc);
    }
}
```

### 2. AMX（Advanced Matrix Extensions）

**功能描述**：矩阵乘法加速。

**相关指令**：
- `TILECONFIG`：配置 tile
- `TILELOADD`：加载 tile
- `TDPBSSD`：矩阵乘法

## 内存管理

### 1. 大页支持

**功能描述**：2MB 和 1GB 页面支持。

**控制方法**：
- 页表项中的 PS 位
- `mmap` 与 `MAP_HUGETLB` 标志

### 2. MTRR（Memory Type Range Registers）

**功能描述**：设置内存区域的缓存策略。

**相关 MSR**：
- `0x2FF`（IA32_MTRR_DEF_TYPE）
- `0x200-0x20F`（IA32_MTRR_PHYSBASEn/PHYSMASKn）

### 3. PAT（Page Attribute Table）

**功能描述**：页级别的内存类型控制。

**控制方法**：
- MSR `0x277`（IA32_PAT）
- 页表项中的 PAT、PCD、PWT 位

## 调试和追踪

### 1. 硬件断点

**功能描述**：数据和指令断点。

**相关寄存器**：
- `DR0-DR3`：断点地址
- `DR6`：调试状态
- `DR7`：调试控制

### 2. Intel PT（Processor Trace）

**功能描述**：硬件级别的控制流追踪。

**相关 MSR**：
- `0x570`（IA32_RTIT_CTL）：PT 控制
- `0x571-0x572`（IA32_RTIT_STATUS）：PT 状态

### 3. LBR（Last Branch Record）

**功能描述**：记录最近的分支。

**相关 MSR**：
- `0x1C9`（IA32_LASTBRANCHFROMIP）
- `0x1CA`（IA32_LASTBRANCHTOIP）

## 其他高级特性

### 1. TSX（Transactional Synchronization Extensions）

**功能描述**：硬件事务内存。

**相关指令**：
- RTM：`XBEGIN`、`XEND`、`XABORT`
- HLE：`XACQUIRE`、`XRELEASE` 前缀

### 2. MPX（Memory Protection Extensions）

**功能描述**：边界检查。

**相关指令**：
- `BNDMK`：创建边界
- `BNDCL/BNDCU`：检查边界

### 3. 内存加密

**Intel TME（Total Memory Encryption）**：
- MSR 配置密钥
- 自动加密所有内存

**AMD SME（Secure Memory Encryption）**：
- C 位在页表项中
- MSR `0xC0010010`（SYSCFG）

### 4. QoS（Quality of Service）

**Intel RDT（Resource Director Technology）**：
- MBA（Memory Bandwidth Allocation）
- CAT（Cache Allocation Technology）
- CMT（Cache Monitoring Technology）
- MBM（Memory Bandwidth Monitoring）

**控制方法**：
- MSR `0xC90-0xD0F`：各种 QoS 控制寄存器

### 5. 功耗监控

**RAPL（Running Average Power Limit）**：
- MSR `0x610`（PKG_POWER_LIMIT）：包功耗限制
- MSR `0x611`（PKG_ENERGY_STATUS）：能耗状态
- MSR `0x639`（PP0_ENERGY_STATUS）：核心能耗

## 编程注意事项

1. **特权级别**：大多数 MSR 访问需要 Ring 0 权限
2. **CPUID 检查**：使用功能前应通过 CPUID 检查可用性
3. **平台差异**：Intel 和 AMD 的实现可能不同
4. **内核支持**：某些功能需要操作系统支持
5. **性能影响**：某些操作（如 WBINVD）可能严重影响性能

## 测试方法

可以通过以下方式测试 CPU 功能：

1. **CPUID 指令**：查询 CPU 能力
2. **读取 MSR**：检查功能状态
3. **执行特定指令**：测试是否产生异常
4. **性能计数器**：验证功能效果
5. **内核模块**：测试需要特权的功能

本文档提供了现代 CPU 可编程功能的全面概述。具体实现细节请参考处理器厂商的编程手册。