#include "common.h"
#include <time.h>
#include <sys/time.h>
#include <sys/sysinfo.h>

int check_root_permission(void) {
    if (geteuid() != 0) {
        PRINT_ERROR("This program requires root privileges");
        return ERROR_PERMISSION;
    }
    return SUCCESS;
}

int check_file_exists(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return SUCCESS;
    }
    return ERROR_SYSTEM;
}

int read_file_int(const char *path, int *value) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        PRINT_ERROR("Failed to open file %s: %s", path, strerror(errno));
        return ERROR_SYSTEM;
    }
    
    if (fscanf(fp, "%d", value) != 1) {
        PRINT_ERROR("Failed to read integer from %s", path);
        fclose(fp);
        return ERROR_SYSTEM;
    }
    
    fclose(fp);
    return SUCCESS;
}

int write_file_int(const char *path, int value) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        PRINT_ERROR("Failed to open file %s for writing: %s", path, strerror(errno));
        return ERROR_SYSTEM;
    }
    
    if (fprintf(fp, "%d", value) < 0) {
        PRINT_ERROR("Failed to write integer to %s", path);
        fclose(fp);
        return ERROR_SYSTEM;
    }
    
    fclose(fp);
    return SUCCESS;
}

int read_file_str(const char *path, char *buffer, size_t size) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        PRINT_ERROR("Failed to open file %s: %s", path, strerror(errno));
        return ERROR_SYSTEM;
    }
    
    if (fgets(buffer, size, fp) == NULL) {
        PRINT_ERROR("Failed to read string from %s", path);
        fclose(fp);
        return ERROR_SYSTEM;
    }
    
    // Remove trailing newline
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len-1] == '\n') {
        buffer[len-1] = '\0';
    }
    
    fclose(fp);
    return SUCCESS;
}

int write_file_str(const char *path, const char *str) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        PRINT_ERROR("Failed to open file %s for writing: %s", path, strerror(errno));
        return ERROR_SYSTEM;
    }
    
    if (fprintf(fp, "%s", str) < 0) {
        PRINT_ERROR("Failed to write string to %s", path);
        fclose(fp);
        return ERROR_SYSTEM;
    }
    
    fclose(fp);
    return SUCCESS;
}

int get_cpu_count(void) {
    return get_nprocs();
}

int get_cpu_vendor(char *vendor, size_t size) {
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (!fp) {
        PRINT_ERROR("Failed to open /proc/cpuinfo");
        return ERROR_SYSTEM;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "vendor_id", 9) == 0) {
            char *colon = strchr(line, ':');
            if (colon) {
                colon++;
                while (*colon == ' ' || *colon == '\t') colon++;
                strncpy(vendor, colon, size - 1);
                vendor[size - 1] = '\0';
                
                // Remove trailing newline
                size_t len = strlen(vendor);
                if (len > 0 && vendor[len-1] == '\n') {
                    vendor[len-1] = '\0';
                }
                
                fclose(fp);
                return SUCCESS;
            }
        }
    }
    
    fclose(fp);
    return ERROR_SYSTEM;
}

int check_cpu_feature(const char *feature) {
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (!fp) {
        PRINT_ERROR("Failed to open /proc/cpuinfo");
        return ERROR_SYSTEM;
    }
    
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "flags", 5) == 0) {
            if (strstr(line, feature)) {
                fclose(fp);
                return SUCCESS;
            }
        }
    }
    
    fclose(fp);
    return ERROR_NOT_SUPPORTED;
}

uint64_t get_timestamp_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

void sleep_ms(int ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}