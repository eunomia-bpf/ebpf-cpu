#include "prefetch_common.h"

int prefetch_check_support(void) {
    // Check if we're running on Intel CPU
    char vendor[64];
    if (get_cpu_vendor(vendor, sizeof(vendor)) != SUCCESS) {
        PRINT_ERROR("Failed to get CPU vendor");
        return ERROR_SYSTEM;
    }
    
    if (strstr(vendor, "Intel") == NULL) {
        PRINT_ERROR("Prefetch control is Intel-specific");
        return ERROR_NOT_SUPPORTED;
    }
    
    // Check MSR availability
    if (msr_check_available() != SUCCESS) {
        return ERROR_NOT_SUPPORTED;
    }
    
    // Try to read the prefetch control MSR
    uint64_t config;
    if (msr_read_cpu(0, MSR_MISC_FEATURES_ENABLES, &config) != SUCCESS) {
        // Try alternative MSR for older CPUs
        if (msr_read_cpu(0, MSR_MISC_FEATURE_CONTROL, &config) != SUCCESS) {
            PRINT_ERROR("Failed to read prefetch control MSR");
            return ERROR_NOT_SUPPORTED;
        }
    }
    
    return SUCCESS;
}

int prefetch_read_config(uint64_t *config) {
    if (config == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    // Try modern MSR first
    if (msr_read_cpu(0, MSR_MISC_FEATURES_ENABLES, config) == SUCCESS) {
        return SUCCESS;
    }
    
    // Fall back to older MSR
    return msr_read_cpu(0, MSR_MISC_FEATURE_CONTROL, config);
}

int prefetch_write_config(uint64_t config) {
    // Write to all CPUs
    int cpu_count = get_cpu_count();
    for (int cpu = 0; cpu < cpu_count; cpu++) {
        // Try modern MSR first
        if (msr_write_cpu(cpu, MSR_MISC_FEATURES_ENABLES, config) != SUCCESS) {
            // Fall back to older MSR
            if (msr_write_cpu(cpu, MSR_MISC_FEATURE_CONTROL, config) != SUCCESS) {
                PRINT_ERROR("Failed to write prefetch config to CPU %d", cpu);
                return ERROR_SYSTEM;
            }
        }
    }
    
    return SUCCESS;
}