# CPU C-State 控制工具

## 概述

CPU C-State（CPU 空闲状态）是处理器在空闲时进入的低功耗状态。本工具提供了对 Linux 系统中 CPU 空闲状态的直接控制，帮助用户在功耗和响应延迟之间找到最佳平衡。

## 背景知识

### C-State（空闲状态）

C-State 是 ACPI 定义的 CPU 空闲时的电源状态。不同的 C-State 代表不同深度的睡眠状态：

- **C0**：活跃状态，CPU 正在执行指令
- **C1**：轻度睡眠，CPU 停止执行指令但可以立即唤醒
- **C2**：中度睡眠，停止时钟，唤醒延迟略高
- **C3**：深度睡眠，刷新缓存，唤醒延迟更高
- **C6/C7/C10**：更深的睡眠状态，关闭更多组件，功耗更低但唤醒延迟更高

### 关键参数

1. **延迟（Latency）**：从该状态唤醒所需的时间（微秒）
2. **目标驻留时间（Target Residency）**：进入该状态获得净能源收益所需的最短时间
3. **使用计数（Usage Count）**：进入该状态的次数
4. **驻留时间（Time）**：在该状态中花费的总时间

### Linux CPUIdle 子系统

Linux 内核通过 CPUIdle 子系统管理 CPU 空闲状态：

- **空闲调速器（Idle Governor）**：决定选择哪个 C-State
  - `menu`：基于预测的调速器，适合变化的工作负载
  - `ladder`：简单的阶梯式调速器，适合稳定的工作负载
  - `teo`：定时器事件导向调速器，更现代的预测算法
- **驱动程序**：与硬件交互，实际进入/退出 C-State

## 主要功能

### 1. C-State 管理（cpu_cstate_control）

- **列出可用状态**：显示所有支持的 C-State 及其属性
- **启用/禁用状态**：控制特定 C-State 的可用性
- **设置最大深度**：限制系统可以进入的最深 C-State
- **选择调速器**：选择空闲状态决策算法
- **监控驻留分布**：实时显示各 C-State 的使用情况
- **统计信息**：显示历史使用数据和平均驻留时间

### 2. 性能基准测试（cpu_cstate_benchmark）

测试不同 C-State 配置对系统性能的影响：
- 响应延迟测试
- 功耗测量
- 性能影响评估

## 使用场景

### 1. 低延迟应用

对于需要快速响应的应用（如实时系统、游戏服务器）：
```bash
# 禁用深度 C-State，只保留 C0 和 C1
./cpu_cstate_control max-cstate 1
```

### 2. 节能优化

对于批处理或低负载系统：
```bash
# 启用所有 C-State
./cpu_cstate_control enable 0
./cpu_cstate_control enable 1
./cpu_cstate_control enable 2
# ...启用所有可用状态
```

### 3. 性能调试

监控 C-State 使用情况以发现性能问题：
```bash
# 监控 30 秒的 C-State 分布
./cpu_cstate_control monitor 30
```

## 典型输出解释

```
C-states for CPU 0:
   State           Name                      Description  Latency(us)  Target(us)    Enabled
-------------------------------------------------------------------------------------------
      C0           POLL                         MWAIT 0x0            0           0        Yes
      C1          MWAIT                        MWAIT 0x10            2           2        Yes
      C2     MWAIT 0x20                        MWAIT 0x20           10          20        Yes
      C3     MWAIT 0x60                        MWAIT 0x60           70         100        Yes
```

- **POLL**：轮询状态，CPU 活跃但空转
- **MWAIT**：使用 MWAIT 指令的硬件支持状态
- **Latency**：唤醒延迟，影响响应时间
- **Target**：建议的最小驻留时间

## 优化建议

1. **实时应用**：
   - 禁用 C2 及更深状态
   - 使用 `ladder` 调速器以获得可预测行为

2. **Web 服务器**：
   - 保留所有 C-State 但使用 `menu` 调速器
   - 根据负载模式调整

3. **HPC 计算**：
   - 计算阶段禁用深度 C-State
   - I/O 等待阶段启用所有 C-State

4. **笔记本/移动设备**：
   - 启用所有 C-State 以最大化电池寿命
   - 使用 `teo` 调速器获得更好的预测

## 注意事项

- 需要 root 权限才能修改 C-State 设置
- 禁用深度 C-State 会增加空闲功耗
- 某些硬件可能不支持所有 C-State
- C-State 配置可能受 BIOS 设置影响
- 过度限制 C-State 可能导致系统过热