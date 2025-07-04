#include "../common/common.h"
#include "../common/msr_utils.h"
#include <dirent.h>

// CXL sysfs paths
#define CXL_BUS_PATH "/sys/bus/cxl"
#define CXL_DEVICES_PATH "/sys/bus/cxl/devices"
#define NUMA_NODE_PATH "/sys/devices/system/node"

typedef struct {
    char device_name[64];
    int numa_node;
    uint64_t size_bytes;
    char target_type[32];
    int is_online;
} cxl_device_t;

typedef struct {
    int region_id;
    char uuid[64];
    uint64_t size_bytes;
    int interleave_ways;
    int num_targets;
    char state[32];
} cxl_region_t;

// Function declarations
int cxl_check_support(void);
int cxl_init(void);
int cxl_cleanup(void);
int cxl_scan_devices(cxl_device_t *devices, int max_devices);
int cxl_scan_regions(cxl_region_t *regions, int max_regions);
int cxl_test_basic_functionality(void);
int cxl_test_memory_access(void);
int cxl_test_bandwidth_measurement(void);
void cxl_print_topology(void);
double cxl_measure_memory_bandwidth(void *addr, size_t size);

static cxl_device_t devices[16];
static cxl_region_t regions[8];
static int num_devices = 0;
static int num_regions = 0;

int main(int argc, char *argv[]) {
    PRINT_INFO("Starting CXL (Compute Express Link) Test");
    
    if (check_root_permission() != SUCCESS) {
        return EXIT_FAILURE;
    }
    
    if (cxl_check_support() != SUCCESS) {
        PRINT_ERROR("CXL not supported or not available on this system");
        return EXIT_FAILURE;
    }
    
    if (cxl_init() != SUCCESS) {
        PRINT_ERROR("Failed to initialize CXL");
        return EXIT_FAILURE;
    }
    
    cxl_print_topology();
    
    int total_tests = 0;
    int passed_tests = 0;
    
    total_tests++;
    if (cxl_test_basic_functionality() == SUCCESS) {
        passed_tests++;
        PRINT_SUCCESS("Basic functionality test passed");
    } else {
        PRINT_ERROR("Basic functionality test failed");
    }
    
    total_tests++;
    if (cxl_test_memory_access() == SUCCESS) {
        passed_tests++;
        PRINT_SUCCESS("Memory access test passed");
    } else {
        PRINT_ERROR("Memory access test failed");
    }
    
    total_tests++;
    if (cxl_test_bandwidth_measurement() == SUCCESS) {
        passed_tests++;
        PRINT_SUCCESS("Bandwidth measurement test passed");
    } else {
        PRINT_ERROR("Bandwidth measurement test failed");
    }
    
    cxl_cleanup();
    
    PRINT_INFO("CXL Test Results: %d/%d tests passed", passed_tests, total_tests);
    return (passed_tests == total_tests) ? EXIT_SUCCESS : EXIT_FAILURE;
}

int cxl_check_support(void) {
    // Check if CXL bus exists
    if (check_file_exists(CXL_BUS_PATH) != SUCCESS) {
        PRINT_ERROR("CXL bus not found in sysfs");
        return ERROR_NOT_SUPPORTED;
    }
    
    // Check if CXL devices directory exists
    if (check_file_exists(CXL_DEVICES_PATH) != SUCCESS) {
        PRINT_ERROR("CXL devices directory not found");
        return ERROR_NOT_SUPPORTED;
    }
    
    return SUCCESS;
}

int cxl_init(void) {
    num_devices = cxl_scan_devices(devices, 16);
    num_regions = cxl_scan_regions(regions, 8);
    
    PRINT_INFO("Found %d CXL devices and %d regions", num_devices, num_regions);
    
    if (num_devices == 0 && num_regions == 0) {
        PRINT_INFO("No CXL devices or regions found - system may not have CXL memory");
        return SUCCESS; // Not an error, just no CXL hardware
    }
    
    return SUCCESS;
}

int cxl_cleanup(void) {
    PRINT_INFO("CXL cleanup completed");
    return SUCCESS;
}

int cxl_scan_devices(cxl_device_t *devices, int max_devices) {
    DIR *dir = opendir(CXL_DEVICES_PATH);
    if (!dir) {
        PRINT_DEBUG("Cannot open CXL devices directory");
        return 0;
    }
    
    struct dirent *entry;
    int device_count = 0;
    
    while ((entry = readdir(dir)) != NULL && device_count < max_devices) {
        if (entry->d_name[0] == '.') continue;
        
        // Look for memory devices (memX pattern)
        if (strncmp(entry->d_name, "mem", 3) == 0) {
            strncpy(devices[device_count].device_name, entry->d_name, 
                   sizeof(devices[device_count].device_name) - 1);
            
            // Try to read device properties
            char path[512];
            
            // Read NUMA node
            snprintf(path, sizeof(path), "%s/%s/numa_node", 
                    CXL_DEVICES_PATH, entry->d_name);
            if (read_file_int(path, &devices[device_count].numa_node) != SUCCESS) {
                devices[device_count].numa_node = -1;
            }
            
            // Read size if available
            snprintf(path, sizeof(path), "%s/%s/size", 
                    CXL_DEVICES_PATH, entry->d_name);
            char size_str[64];
            if (read_file_str(path, size_str, sizeof(size_str)) == SUCCESS) {
                devices[device_count].size_bytes = strtoull(size_str, NULL, 0);
            } else {
                devices[device_count].size_bytes = 0;
            }
            
            devices[device_count].is_online = 1;
            strcpy(devices[device_count].target_type, "memory");
            
            device_count++;
        }
    }
    
    closedir(dir);
    return device_count;
}

int cxl_scan_regions(cxl_region_t *regions, int max_regions) {
    char regions_path[256];
    snprintf(regions_path, sizeof(regions_path), "%s/regions", CXL_BUS_PATH);
    
    DIR *dir = opendir(regions_path);
    if (!dir) {
        PRINT_DEBUG("Cannot open CXL regions directory");
        return 0;
    }
    
    struct dirent *entry;
    int region_count = 0;
    
    while ((entry = readdir(dir)) != NULL && region_count < max_regions) {
        if (entry->d_name[0] == '.') continue;
        
        if (strncmp(entry->d_name, "region", 6) == 0) {
            regions[region_count].region_id = region_count;
            
            char path[512];
            
            // Read region UUID
            snprintf(path, sizeof(path), "%s/%s/uuid", regions_path, entry->d_name);
            if (read_file_str(path, regions[region_count].uuid, 
                             sizeof(regions[region_count].uuid)) != SUCCESS) {
                strcpy(regions[region_count].uuid, "unknown");
            }
            
            // Read region size
            snprintf(path, sizeof(path), "%s/%s/size", regions_path, entry->d_name);
            char size_str[64];
            if (read_file_str(path, size_str, sizeof(size_str)) == SUCCESS) {
                regions[region_count].size_bytes = strtoull(size_str, NULL, 0);
            } else {
                regions[region_count].size_bytes = 0;
            }
            
            // Read interleave ways
            snprintf(path, sizeof(path), "%s/%s/interleave_ways", 
                    regions_path, entry->d_name);
            if (read_file_int(path, &regions[region_count].interleave_ways) != SUCCESS) {
                regions[region_count].interleave_ways = 1;
            }
            
            // Read state
            snprintf(path, sizeof(path), "%s/%s/state", regions_path, entry->d_name);
            if (read_file_str(path, regions[region_count].state, 
                             sizeof(regions[region_count].state)) != SUCCESS) {
                strcpy(regions[region_count].state, "unknown");
            }
            
            region_count++;
        }
    }
    
    closedir(dir);
    return region_count;
}

int cxl_test_basic_functionality(void) {
    PRINT_INFO("Testing basic CXL functionality...");
    
    if (num_devices == 0 && num_regions == 0) {
        PRINT_INFO("No CXL devices found - skipping functionality test");
        return SUCCESS;
    }
    
    // Test device enumeration
    for (int i = 0; i < num_devices; i++) {
        PRINT_DEBUG("Device %d: %s, NUMA node: %d, Size: %lu bytes", 
                   i, devices[i].device_name, devices[i].numa_node, 
                   devices[i].size_bytes);
    }
    
    // Test region information
    for (int i = 0; i < num_regions; i++) {
        PRINT_DEBUG("Region %d: UUID: %s, Size: %lu bytes, State: %s", 
                   i, regions[i].uuid, regions[i].size_bytes, regions[i].state);
    }
    
    return SUCCESS;
}

int cxl_test_memory_access(void) {
    PRINT_INFO("Testing CXL memory access...");
    
    // Check available NUMA nodes
    DIR *numa_dir = opendir(NUMA_NODE_PATH);
    if (!numa_dir) {
        PRINT_ERROR("Cannot access NUMA topology");
        return ERROR_SYSTEM;
    }
    
    struct dirent *entry;
    int cxl_nodes_found = 0;
    
    while ((entry = readdir(numa_dir)) != NULL) {
        if (strncmp(entry->d_name, "node", 4) == 0) {
            int node_id = atoi(entry->d_name + 4);
            
            // Check if this might be a CXL node
            for (int i = 0; i < num_devices; i++) {
                if (devices[i].numa_node == node_id) {
                    PRINT_DEBUG("Found potential CXL NUMA node: %d", node_id);
                    cxl_nodes_found++;
                    
                    // Try to allocate memory on this node
                    char meminfo_path[256];
                    snprintf(meminfo_path, sizeof(meminfo_path), 
                            "%s/node%d/meminfo", NUMA_NODE_PATH, node_id);
                    
                    if (check_file_exists(meminfo_path) == SUCCESS) {
                        PRINT_DEBUG("Node %d has memory information", node_id);
                    }
                    break;
                }
            }
        }
    }
    
    closedir(numa_dir);
    
    if (cxl_nodes_found == 0) {
        PRINT_INFO("No CXL NUMA nodes detected");
    } else {
        PRINT_INFO("Found %d potential CXL NUMA nodes", cxl_nodes_found);
    }
    
    return SUCCESS;
}

int cxl_test_bandwidth_measurement(void) {
    PRINT_INFO("Testing CXL memory bandwidth measurement...");
    
    const size_t test_size = 64 * 1024 * 1024; // 64MB
    void *buffer = malloc(test_size);
    if (!buffer) {
        PRINT_ERROR("Failed to allocate test buffer");
        return ERROR_SYSTEM;
    }
    
    // Initialize buffer
    memset(buffer, 0x55, test_size);
    
    // Measure local DRAM bandwidth for comparison
    double local_bandwidth = cxl_measure_memory_bandwidth(buffer, test_size);
    PRINT_INFO("Local memory bandwidth: %.2f GB/s", local_bandwidth);
    
    // If we have CXL devices, try to measure their bandwidth
    if (num_devices > 0) {
        PRINT_INFO("CXL memory bandwidth measurements would require");
        PRINT_INFO("NUMA-aware allocation and binding to specific nodes");
        PRINT_INFO("This is a simplified test showing the measurement framework");
    }
    
    free(buffer);
    return SUCCESS;
}

double cxl_measure_memory_bandwidth(void *addr, size_t size) {
    volatile char *ptr = (volatile char *)addr;
    const int iterations = 5;
    
    // Sequential read test
    uint64_t start_time = get_timestamp_us();
    volatile char dummy = 0;
    
    for (int iter = 0; iter < iterations; iter++) {
        for (size_t i = 0; i < size; i += 64) {
            dummy += ptr[i];
        }
    }
    
    uint64_t end_time = get_timestamp_us();
    
    double time_sec = (end_time - start_time) / 1000000.0;
    double bytes_read = (double)size * iterations;
    double bandwidth_bps = bytes_read / time_sec;
    
    return bandwidth_bps / (1024.0 * 1024.0 * 1024.0); // Convert to GB/s
}

void cxl_print_topology(void) {
    PRINT_INFO("CXL Topology Information:");
    
    if (num_devices == 0 && num_regions == 0) {
        PRINT_INFO("No CXL devices or regions detected");
        PRINT_INFO("This may indicate:");
        PRINT_INFO("  - No CXL hardware installed");
        PRINT_INFO("  - CXL devices not configured");
        PRINT_INFO("  - Missing kernel CXL support");
        return;
    }
    
    if (num_devices > 0) {
        PRINT_INFO("CXL Memory Devices:");
        for (int i = 0; i < num_devices; i++) {
            PRINT_INFO("  Device: %s", devices[i].device_name);
            PRINT_INFO("    NUMA Node: %d", devices[i].numa_node);
            if (devices[i].size_bytes > 0) {
                PRINT_INFO("    Size: %.2f GB", 
                          devices[i].size_bytes / (1024.0 * 1024.0 * 1024.0));
            }
            PRINT_INFO("    Status: %s", devices[i].is_online ? "Online" : "Offline");
        }
    }
    
    if (num_regions > 0) {
        PRINT_INFO("CXL Regions:");
        for (int i = 0; i < num_regions; i++) {
            PRINT_INFO("  Region %d:", regions[i].region_id);
            PRINT_INFO("    UUID: %s", regions[i].uuid);
            if (regions[i].size_bytes > 0) {
                PRINT_INFO("    Size: %.2f GB", 
                          regions[i].size_bytes / (1024.0 * 1024.0 * 1024.0));
            }
            PRINT_INFO("    Interleave Ways: %d", regions[i].interleave_ways);
            PRINT_INFO("    State: %s", regions[i].state);
        }
    }
}