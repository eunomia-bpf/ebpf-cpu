# Linux Power Management Tools

This repository contains a collection of user-space tools for directly controlling various power management knobs in Linux systems. Each tool provides fine-grained control over specific power management aspects, along with benchmarks to measure their impact on performance and energy efficiency.

## Overview

These tools allow direct manipulation of:
- **CPU Frequency Scaling (P-states/DVFS)** - Dynamic voltage and frequency scaling
- **CPU Idle States (C-states)** - Sleep state management for power savings
- **Thermal Capping** - Proactive thermal management to prevent throttling
- **GPU Frequency Scaling (DevFreq)** - GPU power and performance management

## Prerequisites

- Linux kernel 5.x or later
- Root/sudo privileges
- C++17 compatible compiler
- Standard development tools (make, g++)
- For GPU benchmark: libGL and X11 libraries

## Building

Build all tools:
```bash
make -C cpu-freq
make -C cpu-cstate
make -C thermal-cap
make -C gpu-devfreq
```

Or build individually:
```bash
cd cpu-freq && make
```

## Tool Details

### 1. CPU Frequency Control (cpu-freq)

Controls CPU frequency scaling through the cpufreq subsystem.

**Features:**
- List available frequencies and governors
- Set frequency governors (performance, powersave, ondemand, etc.)
- Set min/max frequency limits
- Set specific frequencies (userspace governor)
- Real-time frequency monitoring
- Frequency residency statistics

**Usage:**
```bash
sudo ./cpu_freq_control list-freq              # List available frequencies
sudo ./cpu_freq_control set-gov performance    # Set governor
sudo ./cpu_freq_control set-limits 800 3600    # Set freq range (MHz)
sudo ./cpu_freq_control monitor 10             # Monitor for 10 seconds
```

**Benchmark:**
```bash
sudo ./cpu_freq_benchmark
```

The benchmark measures:
- Computational performance (GFLOPS) at different frequencies
- Memory bandwidth impact
- Power efficiency (GFLOPS/Watt)
- Latency characteristics

### 2. CPU C-State Control (cpu-cstate)

Manages CPU idle states through the cpuidle subsystem.

**Features:**
- List available C-states with properties
- Enable/disable specific C-states
- Set maximum C-state depth
- Monitor C-state residency
- Control idle governors

**Usage:**
```bash
sudo ./cpu_cstate_control list                 # List C-states
sudo ./cpu_cstate_control disable 3            # Disable C3 state
sudo ./cpu_cstate_control max-cstate 2         # Limit to C2
sudo ./cpu_cstate_control monitor 10           # Monitor for 10 seconds
```

**Benchmark:**
```bash
sudo ./cpu_cstate_benchmark
```

The benchmark evaluates:
- Wake-up latency for different C-state configurations
- Idle power consumption
- Performance impact on intermittent workloads
- Energy efficiency trade-offs

### 3. Thermal Cap Control (thermal-cap)

Implements proactive thermal management by dynamically adjusting CPU frequency based on temperature.

**Features:**
- Monitor thermal zones and temperatures
- Control cooling devices
- Set temperature-based frequency caps
- Implement custom thermal policies
- Real-time thermal monitoring

**Usage:**
```bash
sudo ./thermal_cap_control list                    # List thermal info
sudo ./thermal_cap_control policy 70 80 90         # Set thresholds (°C)
sudo ./thermal_cap_control monitor                 # Monitor and apply caps
sudo ./thermal_cap_control set-cap 2000           # Manual freq cap (MHz)
```

**Benchmark:**
```bash
sudo ./thermal_cap_benchmark
```

The benchmark compares:
- Temperature response curves
- Performance vs temperature trade-offs
- Proactive vs reactive throttling effectiveness
- Thermal variance and stability

### 4. GPU DevFreq Control (gpu-devfreq)

Controls GPU frequency scaling through the devfreq framework.

**Features:**
- List GPU devices with devfreq support
- Set frequency governors
- Set min/max frequency ranges
- Performance and powersave presets
- GPU frequency monitoring
- Support for Intel, AMD, and NVIDIA (nouveau) GPUs

**Usage:**
```bash
sudo ./gpu_devfreq_control list                    # List GPU devices
sudo ./gpu_devfreq_control set-gov 0 performance  # Set governor
sudo ./gpu_devfreq_control set-freq 0 300 1200    # Set freq range (MHz)
sudo ./gpu_devfreq_control monitor 30              # Monitor for 30 seconds
```

**Benchmark:**
```bash
sudo ./gpu_devfreq_benchmark
```

The benchmark measures:
- GPU performance at different frequencies
- CPU-GPU coordination impact
- Power efficiency for various workloads
- Workload scaling characteristics

## Benchmark Results Interpretation

### CPU Frequency Benchmark
- **GFLOPS**: Higher is better - raw computational throughput
- **Memory Bandwidth**: May not scale linearly with frequency
- **Power**: Lower is better for same performance
- **GFLOPS/Watt**: Higher is better - energy efficiency metric

### C-State Benchmark
- **Wake-up Latency**: Lower is better - responsiveness
- **Idle Power**: Lower is better - energy savings
- **Throughput**: Higher is better - performance impact
- **Energy/Op**: Lower is better - efficiency per operation

### Thermal Cap Benchmark
- **Temperature Variance**: Lower is better - thermal stability
- **Throttle Events**: Fewer is better - consistent performance
- **Performance Score**: Higher is better - sustained performance
- **Perf/Joule**: Higher is better - thermal efficiency

### GPU DevFreq Benchmark
- **FPS**: Higher is better - graphics performance
- **FPS/Watt**: Higher is better - GPU efficiency
- **CPU-GPU Balance**: Look for optimal coordination

## Safety Considerations

1. **Temperature**: Monitor temperatures when disabling thermal protections
2. **Voltage**: Frequency changes may affect system stability
3. **Persistence**: Settings are not persistent across reboots
4. **Hardware Limits**: Respect manufacturer specifications

## Integration with eBPF

These tools demonstrate direct user-space control. For eBPF integration:

1. Use these tools to understand baseline behavior
2. Implement similar control logic in eBPF programs
3. Attach eBPF programs to appropriate hooks (cpufreq_ext, thermal events, etc.)
4. Use benchmarks to validate eBPF implementations

## Common Use Cases

### Maximum Performance
```bash
sudo ./cpu_freq_control set-gov performance
sudo ./cpu_cstate_control max-cstate 1
sudo ./gpu_devfreq_control performance 0
```

### Maximum Power Savings
```bash
sudo ./cpu_freq_control set-gov powersave
sudo ./cpu_cstate_control enable 3
sudo ./gpu_devfreq_control powersave 0
```

### Balanced Operation
```bash
sudo ./cpu_freq_control set-gov schedutil
sudo ./thermal_cap_control policy 70 80 90
```

## Troubleshooting

1. **Permission Denied**: Run with sudo
2. **File Not Found**: Check if required kernel modules are loaded
3. **No GPU Devices**: Ensure devfreq support for your GPU
4. **RAPL Not Available**: Energy monitoring may not work on all systems

## Notes

- All tools require root privileges to modify system settings
- Changes are temporary and reset on reboot
- Some features depend on hardware and driver support
- Energy measurements require Intel RAPL or equivalent

## Contributing

Feel free to extend these tools with:
- Additional vendor-specific features
- More sophisticated benchmarks
- Integration with system monitoring tools
- Automated tuning algorithms