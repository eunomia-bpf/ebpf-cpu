#ifndef MSR_UTILS_H
#define MSR_UTILS_H

#include <stdint.h>
#include "common.h"

// MSR device path
#define MSR_DEV_PATH "/dev/cpu/%d/msr"

// Common MSR addresses
#define MSR_IA32_PLATFORM_ID        0x17
#define MSR_IA32_APIC_BASE          0x1B
#define MSR_IA32_FEATURE_CONTROL    0x3A
#define MSR_IA32_TSC                0x10
#define MSR_IA32_MISC_ENABLE        0x1A0
#define MSR_IA32_ENERGY_PERF_BIAS   0x1B0
#define MSR_IA32_PERF_CTL           0x199
#define MSR_IA32_PERF_STATUS        0x198
#define MSR_IA32_CLOCK_MODULATION   0x19A
#define MSR_IA32_THERM_STATUS       0x19C
#define MSR_IA32_THERM_INTERRUPT    0x19B
#define MSR_IA32_TEMPERATURE_TARGET 0x1A2

// RDT MSRs
#define MSR_IA32_L3_MASK_0          0xC90
#define MSR_IA32_L3_MASK_1          0xC91
#define MSR_IA32_L3_MASK_2          0xC92
#define MSR_IA32_L3_MASK_3          0xC93
#define MSR_IA32_PQR_ASSOC          0xC8F
#define MSR_IA32_QM_EVTSEL          0xC8D
#define MSR_IA32_QM_CTR             0xC8E
#define MSR_IA32_MBA_THRTL_MSR      0xD50

// Prefetch control MSRs
#define MSR_MISC_FEATURE_CONTROL    0x1A4
#define MSR_PREFETCH_CONTROL        0x1A0

// Uncore MSRs
#define MSR_UNCORE_RATIO_LIMIT      0x620
#define MSR_UNCORE_PERF_STATUS      0x621

// RAPL MSRs
#define MSR_PKG_POWER_LIMIT         0x610
#define MSR_PKG_ENERGY_STATUS       0x611
#define MSR_PKG_PERF_STATUS         0x613
#define MSR_PKG_POWER_INFO          0x614
#define MSR_DRAM_POWER_LIMIT        0x618
#define MSR_DRAM_ENERGY_STATUS      0x619
#define MSR_DRAM_PERF_STATUS        0x61B
#define MSR_DRAM_POWER_INFO         0x61C
#define MSR_PP0_POWER_LIMIT         0x638
#define MSR_PP0_ENERGY_STATUS       0x639
#define MSR_PP0_POLICY              0x63A
#define MSR_PP0_PERF_STATUS         0x63B
#define MSR_PP1_POWER_LIMIT         0x640
#define MSR_PP1_ENERGY_STATUS       0x641
#define MSR_PP1_POLICY              0x642

// MSR utility functions
int msr_open(int cpu);
void msr_close(int fd);
int msr_read(int fd, uint32_t msr, uint64_t *value);
int msr_write(int fd, uint32_t msr, uint64_t value);
int msr_read_cpu(int cpu, uint32_t msr, uint64_t *value);
int msr_write_cpu(int cpu, uint32_t msr, uint64_t value);

// MSR feature detection
int msr_check_available(void);
int msr_check_cpu_feature(const char *feature);

// MSR field manipulation
uint64_t msr_get_field(uint64_t value, int start_bit, int num_bits);
uint64_t msr_set_field(uint64_t value, int start_bit, int num_bits, uint64_t field_value);

// MSR batch operations
int msr_read_all_cpus(uint32_t msr, uint64_t *values, int max_cpus);
int msr_write_all_cpus(uint32_t msr, uint64_t value, int max_cpus);

#endif /* MSR_UTILS_H */