#!/bin/bash

# RDT Benchmark Wrapper Script
# This script sets up the environment for RDT benchmarking

echo "[INFO] RDT Benchmark Setup Script"

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo "[ERROR] This script must be run as root"
    exit 1
fi

# Load MSR module if not loaded
if ! lsmod | grep -q "^msr"; then
    echo "[INFO] Loading MSR module..."
    modprobe msr
fi

# Check if resctrl is already mounted
if ! mountpoint -q /sys/fs/resctrl; then
    echo "[INFO] Mounting resctrl filesystem..."
    mount -t resctrl resctrl /sys/fs/resctrl 2>/dev/null
    
    if [ $? -ne 0 ]; then
        echo "[WARNING] Failed to mount resctrl. Kernel may not have CONFIG_X86_CPU_RESCTRL enabled."
        echo "[INFO] Attempting to run benchmark with direct MSR access only..."
    else
        echo "[INFO] resctrl mounted successfully"
    fi
else
    echo "[INFO] resctrl already mounted"
fi

# Check kernel config
if [ -f /boot/config-$(uname -r) ]; then
    echo "[INFO] Checking kernel RDT configuration..."
    grep -E "CONFIG_X86_CPU_RESCTRL|CONFIG_INTEL_RDT" /boot/config-$(uname -r) | grep -v "^#"
fi

# Set MSR permissions
echo "[INFO] Setting MSR device permissions..."
chmod 666 /dev/cpu/*/msr 2>/dev/null

# Check if we can write to RDT MSRs
echo "[INFO] Testing MSR write capability..."
# Try to read L3 mask MSR first
if rdmsr 0xc90 >/dev/null 2>&1; then
    echo "[INFO] MSR read successful"
    
    # Try a safe write (all cache ways enabled)
    if wrmsr 0xc90 0xffff 2>/dev/null; then
        echo "[INFO] MSR write successful"
    else
        echo "[WARNING] MSR write failed. This CPU/kernel may restrict direct RDT MSR access."
        echo "[INFO] You may need to:"
        echo "  1. Use kernel with CONFIG_X86_CPU_RESCTRL disabled"
        echo "  2. Add 'rdt=off' to kernel boot parameters"
        echo "  3. Use resctrl interface instead of direct MSR writes"
    fi
else
    echo "[ERROR] Cannot read MSR. Check if rdmsr tool is installed."
fi

echo ""
echo "[INFO] Running original benchmark..."
echo "====================================="

# Run the original benchmark
./build/rdt_bench