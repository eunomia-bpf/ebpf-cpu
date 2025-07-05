# GPU DevFreq 控制工具

## 概述

GPU DevFreq 是 Linux 内核中用于动态调整 GPU 频率的框架，类似于 CPU 的 CPUFreq。本工具提供了对 GPU 频率缩放的直接控制，支持集成显卡和独立显卡，帮助用户在性能和功耗之间找到最佳平衡。

## 背景知识

### DevFreq 框架

DevFreq（Device Frequency）是 Linux 内核的通用设备频率调节框架，主要特点：

1. **设备无关性**：支持各种需要频率调节的设备
2. **策略灵活性**：提供多种调速器（Governor）
3. **统一接口**：通过 sysfs 提供标准化的控制接口

### GPU 频率调节原理

现代 GPU 支持动态频率调节（DVFS）：

1. **性能状态**：GPU 有多个预定义的频率/电压组合
2. **负载监控**：根据 GPU 利用率调整频率
3. **热管理**：温度过高时降低频率
4. **功耗优化**：空闲时降低频率以节省电力

### 支持的 GPU 类型

本工具支持以下类型的 GPU：

1. **Intel 集成显卡**
   - 使用 i915 驱动
   - 支持 Gen4 及更新的架构

2. **AMD GPU**
   - 使用 amdgpu 驱动
   - 支持 GCN 及 RDNA 架构

3. **NVIDIA GPU**
   - 通过 nouveau 开源驱动（功能有限）
   - 专有驱动需使用 nvidia-smi

## 主要功能

### 1. GPU 设备管理（gpu_devfreq_control）

- **列出设备**：显示所有支持 DevFreq 的 GPU
- **频率控制**：设置最小/最大频率范围
- **调速器选择**：选择频率调节策略
- **性能模式**：快速切换到高性能或节能模式
- **实时监控**：监控 GPU 频率变化
- **统计信息**：查看频率转换统计

### 2. 性能基准测试（gpu_devfreq_benchmark）

测试不同频率配置下的 GPU 性能：
- 图形渲染性能
- 计算吞吐量
- 内存带宽
- 功耗测量

## 调速器（Governor）说明

### 1. Simple Ondemand
- 基于 GPU 利用率的简单算法
- 负载高时提升频率，负载低时降低频率
- 适合一般使用场景

### 2. Performance
- 始终使用最高频率
- 最大性能，最高功耗
- 适合游戏和专业应用

### 3. Powersave
- 始终使用最低频率
- 最低功耗，性能受限
- 适合轻度使用和省电模式

### 4. Userspace
- 允许用户空间程序直接控制频率
- 适合自定义频率管理策略

## 使用示例

### 1. 查看 GPU 设备信息

```bash
./gpu_devfreq_control list
```

输出示例：
```
GPU DevFreq Devices:
================================================================================

Device 0: Intel Integrated GPU
  Path: /sys/class/devfreq/0000:00:02.0.gpu
  Current frequency: 750 MHz
  Frequency range: 350 - 1150 MHz
  Available frequencies: 350 450 550 650 750 850 950 1050 1150 MHz
  Current governor: simple_ondemand
  Available governors: simple_ondemand performance powersave userspace
```

### 2. 设置性能模式

用于游戏或图形密集型应用：
```bash
./gpu_devfreq_control performance 0
```

### 3. 设置节能模式

用于日常办公或浏览：
```bash
./gpu_devfreq_control powersave 0
```

### 4. 自定义频率范围

限制 GPU 在特定频率范围内运行：
```bash
# 设置 GPU 0 的频率范围为 450-850 MHz
./gpu_devfreq_control set-freq 0 450 850
```

### 5. 监控 GPU 频率

实时查看 GPU 频率变化：
```bash
./gpu_devfreq_control monitor 60  # 监控 60 秒
```

输出示例：
```
Monitoring GPU frequencies for 60 seconds...

    Time(s)   Intel Integrated GPU(MHz)
------------------------------------------
       0.0                   550
       0.5                   750
       1.0                  1050
       1.5                   950
       2.0                   650
```

### 6. 查看统计信息

```bash
./gpu_devfreq_control stats 0
```

显示频率转换矩阵和其他统计数据。

## 实际应用场景

### 1. 笔记本电脑

延长电池寿命：
```bash
# 日常使用时限制最高频率
./gpu_devfreq_control set-freq 0 350 750

# 插电时恢复全性能
./gpu_devfreq_control set-freq 0 350 1150
```

### 2. 桌面工作站

根据工作负载调整：
```bash
# 3D 渲染时
./gpu_devfreq_control performance 0

# 文档编辑时
./gpu_devfreq_control set-gov 0 simple_ondemand
```

### 3. 服务器/计算节点

优化计算效率：
```bash
# GPGPU 计算时固定高频
./gpu_devfreq_control set-freq 0 1050 1050

# 空闲时允许动态调节
./gpu_devfreq_control set-gov 0 simple_ondemand
```

### 4. HTPC/媒体中心

平衡性能和噪音：
```bash
# 视频播放时的中等性能
./gpu_devfreq_control set-freq 0 550 850
```

## 与其他组件的配合

### 1. 与 CPU 频率协调

高负载时同时提升 CPU 和 GPU 频率：
```bash
# CPU 设置为性能模式
cpu_freq_control set-gov performance

# GPU 也设置为性能模式
gpu_devfreq_control performance 0
```

### 2. 与热管理配合

防止过热：
```bash
# 设置温度驱动的频率上限
thermal_cap_control policy 70 80 90

# GPU 使用动态调节
gpu_devfreq_control set-gov 0 simple_ondemand
```

## 故障排除

### 1. 没有找到 GPU 设备

- 检查驱动是否支持 DevFreq
- 确认内核配置启用了 CONFIG_DEVFREQ_GOV_*
- 某些 GPU 可能需要特定的内核参数

### 2. 频率设置无效

- 检查是否有其他程序控制 GPU（如专有驱动工具）
- 确认频率值在支持范围内
- 某些频率可能因热限制而不可用

### 3. 性能未提升

- GPU 可能受其他因素限制（如内存带宽、温度）
- 应用程序可能不是 GPU 瓶颈
- 检查是否有其他节能设置生效

## 注意事项

- 需要 root 权限访问 DevFreq 接口
- 过高的频率可能导致系统不稳定
- 长时间高频运行需要良好的散热
- 某些 GPU 的 DevFreq 支持可能不完整
- 专有驱动（如 NVIDIA）可能需要使用厂商工具
- 频率变化可能导致短暂的性能波动