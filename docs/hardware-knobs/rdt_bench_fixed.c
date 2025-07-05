#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#define RESCTRL_PATH "/sys/fs/resctrl"

int check_resctrl_support() {
    struct stat st;
    
    // Check if resctrl is mounted
    if (stat(RESCTRL_PATH, &st) == 0) {
        printf("[INFO] resctrl filesystem is available at %s\n", RESCTRL_PATH);
        return 1;
    }
    
    printf("[WARNING] resctrl filesystem not mounted at %s\n", RESCTRL_PATH);
    printf("[INFO] To mount resctrl, run as root:\n");
    printf("       mount -t resctrl resctrl /sys/fs/resctrl\n");
    return 0;
}

int check_msr_permissions() {
    char msr_path[64];
    snprintf(msr_path, sizeof(msr_path), "/dev/cpu/0/msr");
    
    int fd = open(msr_path, O_RDWR);
    if (fd < 0) {
        if (errno == EACCES) {
            printf("[ERROR] No permission to access MSR device %s\n", msr_path);
            printf("[INFO] This program must be run as root or with CAP_SYS_RAWIO capability\n");
        } else if (errno == ENOENT) {
            printf("[ERROR] MSR device %s not found\n", msr_path);
            printf("[INFO] Load the msr module: modprobe msr\n");
        } else {
            printf("[ERROR] Failed to open MSR device: %s\n", strerror(errno));
        }
        return 0;
    }
    
    close(fd);
    printf("[INFO] MSR device access OK\n");
    return 1;
}

int check_rdt_cpu_support() {
    FILE *fp = popen("lscpu | grep -E 'cat_l3|rdt_a|cqm' | wc -l", "r");
    if (!fp) {
        printf("[ERROR] Failed to check CPU features\n");
        return 0;
    }
    
    int count = 0;
    fscanf(fp, "%d", &count);
    pclose(fp);
    
    if (count > 0) {
        printf("[INFO] CPU supports RDT features\n");
        return 1;
    } else {
        printf("[ERROR] CPU does not support RDT features\n");
        return 0;
    }
}

int main() {
    printf("=== RDT Benchmark Diagnostic Tool ===\n\n");
    
    int checks_passed = 0;
    
    // Check CPU support
    if (check_rdt_cpu_support()) {
        checks_passed++;
    }
    
    // Check MSR permissions
    if (check_msr_permissions()) {
        checks_passed++;
    }
    
    // Check resctrl support
    if (check_resctrl_support()) {
        checks_passed++;
    }
    
    printf("\n=== Summary ===\n");
    printf("Checks passed: %d/3\n", checks_passed);
    
    if (checks_passed < 3) {
        printf("\n[ACTION REQUIRED]\n");
        if (!check_resctrl_support()) {
            printf("1. Mount resctrl filesystem (as root):\n");
            printf("   mount -t resctrl resctrl /sys/fs/resctrl\n\n");
        }
        printf("2. If MSR writes still fail after mounting resctrl:\n");
        printf("   - The kernel may be configured to use resctrl interface exclusively\n");
        printf("   - Consider using resctrl interface instead of direct MSR writes\n");
        printf("   - Check kernel config: CONFIG_X86_CPU_RESCTRL=y\n");
        return 1;
    }
    
    printf("\nAll checks passed. RDT should be functional.\n");
    return 0;
}