#include "prefetch_common.h"

// Intel prefetch control MSR definitions
#define MSR_MISC_FEATURES_ENABLES   0x140

// Prefetch control bit definitions
#define PREFETCH_L2_STREAM_HW_DISABLE   (1ULL << 0)
#define PREFETCH_L2_STREAM_ADJ_DISABLE  (1ULL << 1)
#define PREFETCH_DCU_STREAM_DISABLE     (1ULL << 2)
#define PREFETCH_DCU_IP_DISABLE         (1ULL << 3)

// Test parameters
#define TEST_ARRAY_SIZE (16 * 1024 * 1024)  // 16MB
#define TEST_ITERATIONS 10

typedef struct {
    const char *name;
    uint64_t disable_mask;
    const char *description;
} prefetch_config_t;

// Prefetch configurations to test
static prefetch_config_t prefetch_configs[] = {
    {"ALL_ENABLED", 0x0, "All prefetchers enabled"},
    {"L2_STREAM_HW_DISABLED", PREFETCH_L2_STREAM_HW_DISABLE, "L2 stream hardware prefetcher disabled"},
    {"L2_STREAM_ADJ_DISABLED", PREFETCH_L2_STREAM_ADJ_DISABLE, "L2 stream adjacent prefetcher disabled"},
    {"DCU_STREAM_DISABLED", PREFETCH_DCU_STREAM_DISABLE, "DCU stream prefetcher disabled"},
    {"DCU_IP_DISABLED", PREFETCH_DCU_IP_DISABLE, "DCU IP prefetcher disabled"},
    {"ALL_DISABLED", 0xF, "All prefetchers disabled"},
};

// Function declarations
int prefetch_test_basic_functionality(void);
int prefetch_test_performance_impact(void);
int prefetch_benchmark_sequential_access(void);
int prefetch_benchmark_random_access(void);
int prefetch_benchmark_stride_access(int stride);
void prefetch_print_config(uint64_t config);
double prefetch_measure_bandwidth(void *data, size_t size, int pattern);

int main(int argc, char *argv[]) {
    PRINT_INFO("Starting Hardware Prefetch Control Test");
    
    // Check permissions
    if (check_root_permission() != SUCCESS) {
        return EXIT_FAILURE;
    }
    
    // Check prefetch support
    if (prefetch_check_support() != SUCCESS) {
        PRINT_ERROR("Hardware prefetch control not supported");
        return EXIT_FAILURE;
    }
    
    // Print current configuration
    uint64_t current_config;
    if (prefetch_read_config(&current_config) == SUCCESS) {
        PRINT_INFO("Current prefetch configuration:");
        prefetch_print_config(current_config);
    }
    
    // Run tests
    PRINT_INFO("Running prefetch control tests...");
    
    int total_tests = 0;
    int passed_tests = 0;
    
    // Test 1: Basic functionality
    total_tests++;
    if (prefetch_test_basic_functionality() == SUCCESS) {
        passed_tests++;
        PRINT_SUCCESS("Basic functionality test passed");
    } else {
        PRINT_ERROR("Basic functionality test failed");
    }
    
    // Test 2: Performance impact
    total_tests++;
    if (prefetch_test_performance_impact() == SUCCESS) {
        passed_tests++;
        PRINT_SUCCESS("Performance impact test passed");
    } else {
        PRINT_ERROR("Performance impact test failed");
    }
    
    PRINT_INFO("Prefetch Test Results: %d/%d tests passed", passed_tests, total_tests);
    
    return (passed_tests == total_tests) ? EXIT_SUCCESS : EXIT_FAILURE;
}


int prefetch_test_basic_functionality(void) {
    PRINT_INFO("Testing basic prefetch control functionality...");
    
    // Save original configuration
    uint64_t original_config;
    if (prefetch_read_config(&original_config) != SUCCESS) {
        PRINT_ERROR("Failed to read original prefetch configuration");
        return ERROR_SYSTEM;
    }
    
    // Test writing and reading back different configurations
    for (size_t i = 0; i < sizeof(prefetch_configs) / sizeof(prefetch_configs[0]); i++) {
        uint64_t test_config = prefetch_configs[i].disable_mask;
        
        PRINT_DEBUG("Testing configuration: %s", prefetch_configs[i].name);
        
        // Write configuration
        if (prefetch_write_config(test_config) != SUCCESS) {
            PRINT_ERROR("Failed to write prefetch configuration");
            return ERROR_SYSTEM;
        }
        
        // Read back configuration
        uint64_t read_config;
        if (prefetch_read_config(&read_config) != SUCCESS) {
            PRINT_ERROR("Failed to read back prefetch configuration");
            return ERROR_SYSTEM;
        }
        
        // Check if configuration matches (mask only the bits we care about)
        if ((read_config & 0xF) != (test_config & 0xF)) {
            PRINT_ERROR("Configuration mismatch: wrote 0x%lx, read 0x%lx", 
                       test_config & 0xF, read_config & 0xF);
            return ERROR_SYSTEM;
        }
        
        PRINT_DEBUG("Configuration verified: 0x%lx", read_config & 0xF);
    }
    
    // Restore original configuration
    prefetch_write_config(original_config);
    
    return SUCCESS;
}

int prefetch_test_performance_impact(void) {
    PRINT_INFO("Testing performance impact of prefetch control...");
    
    // Allocate test data
    void *data = malloc(TEST_ARRAY_SIZE);
    if (data == NULL) {
        PRINT_ERROR("Failed to allocate test data");
        return ERROR_SYSTEM;
    }
    
    // Initialize data
    memset(data, 0xAA, TEST_ARRAY_SIZE);
    
    // Save original configuration
    uint64_t original_config;
    if (prefetch_read_config(&original_config) != SUCCESS) {
        PRINT_ERROR("Failed to read original prefetch configuration");
        free(data);
        return ERROR_SYSTEM;
    }
    
    PRINT_INFO("Performance comparison:");
    PRINT_INFO("Configuration                    Sequential      Random       Stride-8");
    PRINT_INFO("----------------------------    ----------    ----------    ----------");
    
    // Test each configuration
    for (size_t i = 0; i < sizeof(prefetch_configs) / sizeof(prefetch_configs[0]); i++) {
        uint64_t test_config = prefetch_configs[i].disable_mask;
        
        // Apply configuration
        if (prefetch_write_config(test_config) != SUCCESS) {
            PRINT_ERROR("Failed to apply configuration %s", prefetch_configs[i].name);
            continue;
        }
        
        // Wait for configuration to take effect
        sleep_ms(100);
        
        // Run benchmarks
        double seq_bw = prefetch_measure_bandwidth(data, TEST_ARRAY_SIZE, 0);
        double rand_bw = prefetch_measure_bandwidth(data, TEST_ARRAY_SIZE, 1);
        double stride_bw = prefetch_measure_bandwidth(data, TEST_ARRAY_SIZE, 2);
        
        printf("%-28s    %7.1f MB/s    %7.1f MB/s    %7.1f MB/s\n",
               prefetch_configs[i].name, seq_bw, rand_bw, stride_bw);
    }
    
    // Restore original configuration
    prefetch_write_config(original_config);
    
    free(data);
    return SUCCESS;
}

double prefetch_measure_bandwidth(void *data, size_t size, int pattern) {
    volatile char *ptr = (volatile char *)data;
    uint64_t start_time, end_time;
    volatile char dummy = 0;
    
    // Warm up
    for (size_t i = 0; i < size; i += 64) {
        dummy += ptr[i];
    }
    
    start_time = get_timestamp_us();
    
    switch (pattern) {
        case 0: // Sequential access
            for (int iter = 0; iter < TEST_ITERATIONS; iter++) {
                for (size_t i = 0; i < size; i += 64) {
                    dummy += ptr[i];
                }
            }
            break;
            
        case 1: // Random access
            for (int iter = 0; iter < TEST_ITERATIONS; iter++) {
                for (size_t i = 0; i < size / 1024; i++) {
                    size_t idx = (rand() % (size / 64)) * 64;
                    dummy += ptr[idx];
                }
            }
            break;
            
        case 2: // Stride access (stride = 8 cache lines)
            for (int iter = 0; iter < TEST_ITERATIONS; iter++) {
                for (size_t i = 0; i < size; i += 512) {
                    dummy += ptr[i];
                }
            }
            break;
    }
    
    end_time = get_timestamp_us();
    
    // Calculate bandwidth
    double time_sec = (end_time - start_time) / 1000000.0;
    double bytes_accessed = (double)size * TEST_ITERATIONS;
    if (pattern == 1) {
        bytes_accessed = (double)(size / 1024) * 64 * TEST_ITERATIONS;
    } else if (pattern == 2) {
        bytes_accessed = (double)(size / 512) * 64 * TEST_ITERATIONS;
    }
    
    return (bytes_accessed / (1024 * 1024)) / time_sec;
}

void prefetch_print_config(uint64_t config) {
    PRINT_INFO("Prefetch Configuration (0x%lx):", config & 0xF);
    
    if (config & PREFETCH_L2_STREAM_HW_DISABLE) {
        PRINT_INFO("  - L2 Stream Hardware Prefetcher: DISABLED");
    } else {
        PRINT_INFO("  - L2 Stream Hardware Prefetcher: ENABLED");
    }
    
    if (config & PREFETCH_L2_STREAM_ADJ_DISABLE) {
        PRINT_INFO("  - L2 Stream Adjacent Prefetcher: DISABLED");
    } else {
        PRINT_INFO("  - L2 Stream Adjacent Prefetcher: ENABLED");
    }
    
    if (config & PREFETCH_DCU_STREAM_DISABLE) {
        PRINT_INFO("  - DCU Stream Prefetcher: DISABLED");
    } else {
        PRINT_INFO("  - DCU Stream Prefetcher: ENABLED");
    }
    
    if (config & PREFETCH_DCU_IP_DISABLE) {
        PRINT_INFO("  - DCU IP Prefetcher: DISABLED");
    } else {
        PRINT_INFO("  - DCU IP Prefetcher: ENABLED");
    }
}