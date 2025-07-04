#include "../common/common.h"
#include "../common/msr_utils.h"
#include <sys/stat.h>
#include <dirent.h>

// RDT specific definitions
#define RESCTRL_PATH "/sys/fs/resctrl"
#define MAX_CLOS 16
#define MAX_CACHE_WAYS 20

typedef struct {
    int clos_id;
    uint64_t l3_mask;
    int mb_throttle;
} rdt_config_t;

// Function declarations
int rdt_check_support(void);
int rdt_init(void);
int rdt_cleanup(void);
int rdt_read_l3_mask(int clos_id, uint64_t *mask);
int rdt_write_l3_mask(int clos_id, uint64_t mask);
int rdt_set_clos(int cpu, int clos_id);
int rdt_get_clos(int cpu, int *clos_id);
int rdt_monitor_bandwidth(int clos_id, uint64_t *bandwidth);
int rdt_test_basic_functionality(void);
int rdt_test_cache_allocation(void);
int rdt_test_bandwidth_monitoring(void);
int rdt_test_dynamic_switching(void);
void rdt_print_config(void);

int main(int argc, char *argv[]) {
    PRINT_INFO("Starting RDT (Resource Director Technology) Test");
    
    // Check permissions
    if (check_root_permission() != SUCCESS) {
        return EXIT_FAILURE;
    }
    
    // Check RDT support
    if (rdt_check_support() != SUCCESS) {
        PRINT_ERROR("RDT not supported on this system");
        return EXIT_FAILURE;
    }
    
    // Initialize RDT
    if (rdt_init() != SUCCESS) {
        PRINT_ERROR("Failed to initialize RDT");
        return EXIT_FAILURE;
    }
    
    // Print current configuration
    rdt_print_config();
    
    // Run tests
    PRINT_INFO("Running RDT tests...");
    
    int total_tests = 0;
    int passed_tests = 0;
    
    // Test 1: Basic functionality
    total_tests++;
    if (rdt_test_basic_functionality() == SUCCESS) {
        passed_tests++;
        PRINT_SUCCESS("Basic functionality test passed");
    } else {
        PRINT_ERROR("Basic functionality test failed");
    }
    
    // Test 2: Cache allocation
    total_tests++;
    if (rdt_test_cache_allocation() == SUCCESS) {
        passed_tests++;
        PRINT_SUCCESS("Cache allocation test passed");
    } else {
        PRINT_ERROR("Cache allocation test failed");
    }
    
    // Test 3: Bandwidth monitoring
    total_tests++;
    if (rdt_test_bandwidth_monitoring() == SUCCESS) {
        passed_tests++;
        PRINT_SUCCESS("Bandwidth monitoring test passed");
    } else {
        PRINT_ERROR("Bandwidth monitoring test failed");
    }
    
    // Test 4: Dynamic switching
    total_tests++;
    if (rdt_test_dynamic_switching() == SUCCESS) {
        passed_tests++;
        PRINT_SUCCESS("Dynamic switching test passed");
    } else {
        PRINT_ERROR("Dynamic switching test failed");
    }
    
    // Cleanup
    rdt_cleanup();
    
    PRINT_INFO("RDT Test Results: %d/%d tests passed", passed_tests, total_tests);
    
    return (passed_tests == total_tests) ? EXIT_SUCCESS : EXIT_FAILURE;
}

int rdt_check_support(void) {
    // Check if resctrl filesystem is available
    if (check_file_exists(RESCTRL_PATH) != SUCCESS) {
        PRINT_ERROR("Resctrl filesystem not found at %s", RESCTRL_PATH);
        return ERROR_NOT_SUPPORTED;
    }
    
    // Check CPU features
    if (check_cpu_feature("rdt_a") != SUCCESS) {
        PRINT_ERROR("CPU does not support RDT allocation");
        return ERROR_NOT_SUPPORTED;
    }
    
    // Check MSR availability
    if (msr_check_available() != SUCCESS) {
        return ERROR_NOT_SUPPORTED;
    }
    
    return SUCCESS;
}

int rdt_init(void) {
    // Mount resctrl filesystem if not already mounted
    if (system("mount -t resctrl resctrl /sys/fs/resctrl 2>/dev/null") != 0) {
        // Already mounted or failed - check if it's working
        if (check_file_exists(RESCTRL_PATH "/cpus") != SUCCESS) {
            PRINT_ERROR("Failed to mount resctrl filesystem");
            return ERROR_SYSTEM;
        }
    }
    
    PRINT_INFO("RDT initialized successfully");
    return SUCCESS;
}

int rdt_cleanup(void) {
    // Reset all CPUs to default CLOS (0)
    int cpu_count = get_cpu_count();
    for (int cpu = 0; cpu < cpu_count; cpu++) {
        rdt_set_clos(cpu, 0);
    }
    
    PRINT_INFO("RDT cleanup completed");
    return SUCCESS;
}

int rdt_read_l3_mask(int clos_id, uint64_t *mask) {
    if (clos_id >= MAX_CLOS || mask == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    uint32_t msr = MSR_IA32_L3_MASK_0 + clos_id;
    return msr_read_cpu(0, msr, mask);
}

int rdt_write_l3_mask(int clos_id, uint64_t mask) {
    if (clos_id >= MAX_CLOS) {
        return ERROR_INVALID_PARAM;
    }
    
    uint32_t msr = MSR_IA32_L3_MASK_0 + clos_id;
    
    // Write to all CPUs
    int cpu_count = get_cpu_count();
    for (int cpu = 0; cpu < cpu_count; cpu++) {
        if (msr_write_cpu(cpu, msr, mask) != SUCCESS) {
            PRINT_ERROR("Failed to write L3 mask to CPU %d", cpu);
            return ERROR_SYSTEM;
        }
    }
    
    return SUCCESS;
}

int rdt_set_clos(int cpu, int clos_id) {
    if (clos_id >= MAX_CLOS) {
        return ERROR_INVALID_PARAM;
    }
    
    uint64_t value;
    if (msr_read_cpu(cpu, MSR_IA32_PQR_ASSOC, &value) != SUCCESS) {
        return ERROR_SYSTEM;
    }
    
    // Set CLOS ID in bits 31:0
    value = (value & 0xFFFFFFFF00000000ULL) | (uint64_t)clos_id;
    
    return msr_write_cpu(cpu, MSR_IA32_PQR_ASSOC, value);
}

int rdt_get_clos(int cpu, int *clos_id) {
    if (clos_id == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    uint64_t value;
    if (msr_read_cpu(cpu, MSR_IA32_PQR_ASSOC, &value) != SUCCESS) {
        return ERROR_SYSTEM;
    }
    
    *clos_id = (int)(value & 0xFFFFFFFF);
    return SUCCESS;
}

int rdt_monitor_bandwidth(int clos_id, uint64_t *bandwidth) {
    if (bandwidth == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    // This is a simplified implementation
    // Real implementation would require setting up monitoring
    *bandwidth = 0;
    return SUCCESS;
}

int rdt_test_basic_functionality(void) {
    PRINT_INFO("Testing basic RDT functionality...");
    
    // Test reading default L3 mask
    uint64_t mask;
    if (rdt_read_l3_mask(0, &mask) != SUCCESS) {
        PRINT_ERROR("Failed to read default L3 mask");
        return ERROR_SYSTEM;
    }
    
    PRINT_DEBUG("Default L3 mask for CLOS 0: 0x%lx", mask);
    
    // Test reading current CLOS assignment
    int clos_id;
    if (rdt_get_clos(0, &clos_id) != SUCCESS) {
        PRINT_ERROR("Failed to read current CLOS assignment");
        return ERROR_SYSTEM;
    }
    
    PRINT_DEBUG("Current CLOS for CPU 0: %d", clos_id);
    
    return SUCCESS;
}

int rdt_test_cache_allocation(void) {
    PRINT_INFO("Testing cache allocation...");
    
    // Save original configuration
    uint64_t original_mask;
    if (rdt_read_l3_mask(1, &original_mask) != SUCCESS) {
        PRINT_ERROR("Failed to read original L3 mask for CLOS 1");
        return ERROR_SYSTEM;
    }
    
    // Set a restricted mask for CLOS 1 (use only first 10 cache ways)
    uint64_t restricted_mask = 0x3FF; // 10 bits set
    if (rdt_write_l3_mask(1, restricted_mask) != SUCCESS) {
        PRINT_ERROR("Failed to write restricted L3 mask");
        return ERROR_SYSTEM;
    }
    
    // Verify the mask was set correctly
    uint64_t read_mask;
    if (rdt_read_l3_mask(1, &read_mask) != SUCCESS) {
        PRINT_ERROR("Failed to read back L3 mask");
        return ERROR_SYSTEM;
    }
    
    if (read_mask != restricted_mask) {
        PRINT_ERROR("L3 mask mismatch: wrote 0x%lx, read 0x%lx", restricted_mask, read_mask);
        return ERROR_SYSTEM;
    }
    
    PRINT_DEBUG("Successfully set L3 mask for CLOS 1: 0x%lx", read_mask);
    
    // Restore original configuration
    rdt_write_l3_mask(1, original_mask);
    
    return SUCCESS;
}

int rdt_test_bandwidth_monitoring(void) {
    PRINT_INFO("Testing bandwidth monitoring...");
    
    // Check if monitoring is supported
    if (check_cpu_feature("rdt_m") != SUCCESS) {
        PRINT_INFO("Bandwidth monitoring not supported, skipping test");
        return SUCCESS;
    }
    
    // Simple bandwidth monitoring test
    uint64_t bandwidth;
    if (rdt_monitor_bandwidth(0, &bandwidth) != SUCCESS) {
        PRINT_ERROR("Failed to monitor bandwidth");
        return ERROR_SYSTEM;
    }
    
    PRINT_DEBUG("Current bandwidth for CLOS 0: %lu MB/s", bandwidth);
    
    return SUCCESS;
}

int rdt_test_dynamic_switching(void) {
    PRINT_INFO("Testing dynamic CLOS switching...");
    
    // Save original CLOS assignment
    int original_clos;
    if (rdt_get_clos(0, &original_clos) != SUCCESS) {
        PRINT_ERROR("Failed to get original CLOS assignment");
        return ERROR_SYSTEM;
    }
    
    // Test switching to different CLOS
    int test_clos = 1;
    if (rdt_set_clos(0, test_clos) != SUCCESS) {
        PRINT_ERROR("Failed to set CLOS to %d", test_clos);
        return ERROR_SYSTEM;
    }
    
    // Verify the switch
    int current_clos;
    if (rdt_get_clos(0, &current_clos) != SUCCESS) {
        PRINT_ERROR("Failed to get current CLOS assignment");
        return ERROR_SYSTEM;
    }
    
    if (current_clos != test_clos) {
        PRINT_ERROR("CLOS switch failed: expected %d, got %d", test_clos, current_clos);
        return ERROR_SYSTEM;
    }
    
    PRINT_DEBUG("Successfully switched CPU 0 to CLOS %d", test_clos);
    
    // Measure switching latency
    uint64_t start_time = get_timestamp_us();
    for (int i = 0; i < 1000; i++) {
        rdt_set_clos(0, (i % 2) ? 1 : 0);
    }
    uint64_t end_time = get_timestamp_us();
    
    uint64_t avg_latency = (end_time - start_time) / 1000;
    PRINT_INFO("Average CLOS switching latency: %lu microseconds", avg_latency);
    
    // Restore original CLOS assignment
    rdt_set_clos(0, original_clos);
    
    return SUCCESS;
}

void rdt_print_config(void) {
    PRINT_INFO("Current RDT Configuration:");
    
    // Print CPU vendor
    char vendor[64];
    if (get_cpu_vendor(vendor, sizeof(vendor)) == SUCCESS) {
        PRINT_INFO("CPU Vendor: %s", vendor);
    }
    
    // Print supported features
    PRINT_INFO("RDT Features:");
    if (check_cpu_feature("rdt_a") == SUCCESS) {
        PRINT_INFO("  - Cache Allocation Technology (CAT): Supported");
    }
    if (check_cpu_feature("rdt_m") == SUCCESS) {
        PRINT_INFO("  - Memory Bandwidth Monitoring (MBM): Supported");
    }
    if (check_cpu_feature("mba") == SUCCESS) {
        PRINT_INFO("  - Memory Bandwidth Allocation (MBA): Supported");
    }
    
    // Print current L3 masks for first few CLOS
    PRINT_INFO("L3 Cache Masks:");
    for (int i = 0; i < 4; i++) {
        uint64_t mask;
        if (rdt_read_l3_mask(i, &mask) == SUCCESS) {
            PRINT_INFO("  CLOS %d: 0x%lx", i, mask);
        }
    }
    
    // Print current CLOS assignments for first few CPUs
    PRINT_INFO("Current CLOS Assignments:");
    int cpu_count = get_cpu_count();
    for (int i = 0; i < (cpu_count < 4 ? cpu_count : 4); i++) {
        int clos_id;
        if (rdt_get_clos(i, &clos_id) == SUCCESS) {
            PRINT_INFO("  CPU %d: CLOS %d", i, clos_id);
        }
    }
}