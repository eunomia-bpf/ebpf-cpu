#include "msr_utils.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

int msr_open(int cpu) {
    char path[256];
    snprintf(path, sizeof(path), MSR_DEV_PATH, cpu);
    
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        PRINT_ERROR("Failed to open MSR device for CPU %d: %s", cpu, strerror(errno));
        return -1;
    }
    
    return fd;
}

void msr_close(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

int msr_read(int fd, uint32_t msr, uint64_t *value) {
    if (fd < 0 || value == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    if (lseek(fd, msr, SEEK_SET) < 0) {
        PRINT_ERROR("Failed to seek MSR 0x%x: %s", msr, strerror(errno));
        return ERROR_SYSTEM;
    }
    
    ssize_t result = read(fd, value, sizeof(uint64_t));
    if (result != sizeof(uint64_t)) {
        PRINT_ERROR("Failed to read MSR 0x%x: %s", msr, strerror(errno));
        return ERROR_SYSTEM;
    }
    
    return SUCCESS;
}

int msr_write(int fd, uint32_t msr, uint64_t value) {
    if (fd < 0) {
        return ERROR_INVALID_PARAM;
    }
    
    if (lseek(fd, msr, SEEK_SET) < 0) {
        PRINT_ERROR("Failed to seek MSR 0x%x: %s", msr, strerror(errno));
        return ERROR_SYSTEM;
    }
    
    ssize_t result = write(fd, &value, sizeof(uint64_t));
    if (result != sizeof(uint64_t)) {
        PRINT_ERROR("Failed to write MSR 0x%x: %s", msr, strerror(errno));
        return ERROR_SYSTEM;
    }
    
    return SUCCESS;
}

int msr_read_cpu(int cpu, uint32_t msr, uint64_t *value) {
    int fd = msr_open(cpu);
    if (fd < 0) {
        return ERROR_SYSTEM;
    }
    
    int result = msr_read(fd, msr, value);
    msr_close(fd);
    
    return result;
}

int msr_write_cpu(int cpu, uint32_t msr, uint64_t value) {
    int fd = msr_open(cpu);
    if (fd < 0) {
        return ERROR_SYSTEM;
    }
    
    int result = msr_write(fd, msr, value);
    msr_close(fd);
    
    return result;
}

int msr_check_available(void) {
    // Check if MSR module is loaded
    if (access("/dev/cpu/0/msr", R_OK | W_OK) != 0) {
        PRINT_ERROR("MSR device not available. Try: modprobe msr");
        return ERROR_NOT_SUPPORTED;
    }
    
    return SUCCESS;
}

int msr_check_cpu_feature(const char *feature) {
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (!fp) {
        PRINT_ERROR("Failed to open /proc/cpuinfo");
        return ERROR_SYSTEM;
    }
    
    char line[256];
    int found = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "flags") && strstr(line, feature)) {
            found = 1;
            break;
        }
    }
    
    fclose(fp);
    
    if (!found) {
        PRINT_ERROR("CPU feature '%s' not supported", feature);
        return ERROR_NOT_SUPPORTED;
    }
    
    return SUCCESS;
}

uint64_t msr_get_field(uint64_t value, int start_bit, int num_bits) {
    uint64_t mask = (1ULL << num_bits) - 1;
    return (value >> start_bit) & mask;
}

uint64_t msr_set_field(uint64_t value, int start_bit, int num_bits, uint64_t field_value) {
    uint64_t mask = (1ULL << num_bits) - 1;
    value &= ~(mask << start_bit);
    value |= (field_value & mask) << start_bit;
    return value;
}

int msr_read_all_cpus(uint32_t msr, uint64_t *values, int max_cpus) {
    int cpu_count = get_cpu_count();
    if (cpu_count > max_cpus) {
        cpu_count = max_cpus;
    }
    
    for (int i = 0; i < cpu_count; i++) {
        if (msr_read_cpu(i, msr, &values[i]) != SUCCESS) {
            PRINT_ERROR("Failed to read MSR 0x%x from CPU %d", msr, i);
            return ERROR_SYSTEM;
        }
    }
    
    return cpu_count;
}

int msr_write_all_cpus(uint32_t msr, uint64_t value, int max_cpus) {
    int cpu_count = get_cpu_count();
    if (cpu_count > max_cpus) {
        cpu_count = max_cpus;
    }
    
    for (int i = 0; i < cpu_count; i++) {
        if (msr_write_cpu(i, msr, value) != SUCCESS) {
            PRINT_ERROR("Failed to write MSR 0x%x to CPU %d", msr, i);
            return ERROR_SYSTEM;
        }
    }
    
    return cpu_count;
}